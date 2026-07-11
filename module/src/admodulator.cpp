#include "admodulator.h"
#include "lxcontrolservice.h"

#include <sequenceplayer.h>
#include <algorithm>

RTTI_BEGIN_ENUM(lx::EAdMode)
	RTTI_ENUM_VALUE(lx::EAdMode::OneShot,			"OneShot"),
	RTTI_ENUM_VALUE(lx::EAdMode::LoopWhileSustained,	"LoopWhileSustained")
RTTI_END_ENUM

RTTI_BEGIN_CLASS(lx::AdModulator)
	RTTI_PROPERTY("Attack",	&lx::AdModulator::mAttack,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Decay",	&lx::AdModulator::mDecay,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Mode",	&lx::AdModulator::mMode,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace lx
{
	static constexpr double sMinPhase = 0.001;

	void AdModulator::generateCurve(nap::lxcontrolService& svc)
	{
		const double a = std::max<double>(mAttack, sMinPhase);
		const double d = std::max<double>(mDecay, sMinPhase);
		mDuration = a + d;

		const std::vector<lx::Key> keys = {
			{ 0.0,		0.0f, nap::math::ECurveInterp::Linear },
			{ a,		1.0f, nap::math::ECurveInterp::Linear },
			{ a + d,	0.0f, nap::math::ECurveInterp::Linear }
		};
		svc.authorFloatCurve(*mEditor, mTrackID, keys);
	}


	void AdModulator::onTrigger()
	{
		Modulator::onTrigger();
		if (mPlayer == nullptr)
			return;
		mPlayer->setPlayerTime(0.0);
		mPlayer->setIsLooping(mMode == EAdMode::LoopWhileSustained);
		mPlayer->setIsPlaying(true);
	}


	void AdModulator::onStop()
	{
		Modulator::onStop();
		if (mPlayer == nullptr)
			return;
		// LoopWhileSustained: stop looping so the current A->D cycle finishes then update() stops it.
		mPlayer->setIsLooping(false);
	}


	void AdModulator::update(double deltaTime)
	{
		if (mPlayer == nullptr)
			return;
		// Not looping (OneShot always, or LoopWhileSustained after gate-off): stop at the end.
		if (!mPlayer->getIsLooping() && mPlayer->getIsPlaying() && mPlayer->getPlayerTime() >= mDuration)
			mPlayer->setIsPlaying(false);
	}
}
