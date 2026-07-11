#include "lfomodulator.h"
#include "lxcontrolservice.h"

#include <sequenceplayer.h>
#include <cmath>

RTTI_BEGIN_ENUM(lx::ELfoShape)
	RTTI_ENUM_VALUE(lx::ELfoShape::Sine,		"Sine"),
	RTTI_ENUM_VALUE(lx::ELfoShape::Ramp,		"Ramp"),
	RTTI_ENUM_VALUE(lx::ELfoShape::Triangle,	"Triangle"),
	RTTI_ENUM_VALUE(lx::ELfoShape::Square,		"Square"),
	RTTI_ENUM_VALUE(lx::ELfoShape::Pulse,		"Pulse"),
	RTTI_ENUM_VALUE(lx::ELfoShape::Gaussian,	"Gaussian")
RTTI_END_ENUM

RTTI_BEGIN_ENUM(lx::ELfoMode)
	RTTI_ENUM_VALUE(lx::ELfoMode::Loop,				"Loop"),
	RTTI_ENUM_VALUE(lx::ELfoMode::OneShot,			"OneShot"),
	RTTI_ENUM_VALUE(lx::ELfoMode::LoopRetrigger,	"LoopRetrigger")
RTTI_END_ENUM

RTTI_BEGIN_CLASS(lx::LfoModulator)
	RTTI_PROPERTY("Shape",			&lx::LfoModulator::mShape,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Mode",			&lx::LfoModulator::mMode,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Frequency",		&lx::LfoModulator::mFrequency,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("PulseWidth",		&lx::LfoModulator::mPulseWidth,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("GaussianWidth",	&lx::LfoModulator::mGaussianWidth,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("PhaseOffset",	&lx::LfoModulator::mPhaseOffset,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace lx
{
	// One period spans exactly 1 second; playback speed = frequency turns that into Hz.
	static constexpr double sEdge = 0.01;	// ~10ms fake-vertical edge for square/pulse

	void LfoModulator::generateCurve(nap::lxcontrolService& svc)
	{
		mDuration = 1.0;
		using I = nap::math::ECurveInterp;
		std::vector<lx::Key> keys;

		switch (mShape)
		{
		case ELfoShape::Sine:
			keys = { {0.0,0.5f,I::Bezier}, {0.25,1.0f,I::Bezier}, {0.5,0.5f,I::Bezier},
					 {0.75,0.0f,I::Bezier}, {1.0,0.5f,I::Bezier} };
			break;
		case ELfoShape::Ramp:
			keys = { {0.0,0.0f,I::Linear}, {1.0,1.0f,I::Linear} };
			break;
		case ELfoShape::Triangle:
			keys = { {0.0,0.0f,I::Linear}, {0.5,1.0f,I::Linear}, {1.0,0.0f,I::Linear} };
			break;
		case ELfoShape::Square:
			keys = { {0.0,1.0f,I::Linear}, {0.5,1.0f,I::Linear},
					 {0.5 + sEdge,0.0f,I::Linear}, {1.0,0.0f,I::Linear} };
			break;
		case ELfoShape::Pulse:
		{
			double pw = std::min(std::max((double)mPulseWidth, sEdge), 1.0 - 2.0 * sEdge);
			keys = { {0.0,1.0f,I::Linear}, {pw,1.0f,I::Linear},
					 {pw + sEdge,0.0f,I::Linear}, {1.0,0.0f,I::Linear} };
			break;
		}
		case ELfoShape::Gaussian:
		{
			for (int i = 0; i <= 8; ++i)
			{
				double p = i / 8.0;
				float dd = (float)((p - 0.5) * mGaussianWidth);
				keys.push_back({ p, std::exp(-(dd * dd)), I::Bezier });
			}
			break;
		}
		}
		svc.authorFloatCurve(*mEditor, mTrackID, keys);
	}


	void LfoModulator::onTrigger()
	{
		Modulator::onTrigger();
		if (mPlayer == nullptr)
			return;
		mPlayer->setPlaybackSpeed(mFrequency);
		switch (mMode)
		{
		case ELfoMode::Loop:
			mPlayer->setIsLooping(true);
			if (!mPlayer->getIsPlaying())
				mPlayer->setPlayerTime(mPhaseOffset);
			mPlayer->setIsPlaying(true);
			break;
		case ELfoMode::OneShot:
			mPlayer->setIsLooping(false);
			mPlayer->setPlayerTime(0.0);
			mPlayer->setIsPlaying(true);
			break;
		case ELfoMode::LoopRetrigger:
			mPlayer->setIsLooping(true);
			mPlayer->setPlayerTime(0.0);	// hard phase reset
			mPlayer->setIsPlaying(true);
			break;
		}
	}


	void LfoModulator::update(double deltaTime)
	{
		if (mPlayer == nullptr)
			return;
		// One-shot: stop after a single period.
		if (mMode == ELfoMode::OneShot && mPlayer->getIsPlaying() && mPlayer->getPlayerTime() >= mDuration)
			mPlayer->setIsPlaying(false);
	}


	void LfoModulator::onStop()
	{
		Modulator::onStop();
		if (mPlayer != nullptr)
			mPlayer->setIsPlaying(false);	// free-running: gate-off stops immediately
	}
}
