#include "lfomodulator.h"

#include <cmath>

RTTI_BEGIN_ENUM(lx::ELfoShape)
	RTTI_ENUM_VALUE(lx::ELfoShape::Sine,		"Sine"),
	RTTI_ENUM_VALUE(lx::ELfoShape::Ramp,		"Ramp"),
	RTTI_ENUM_VALUE(lx::ELfoShape::Triangle,	"Triangle"),
	RTTI_ENUM_VALUE(lx::ELfoShape::Square,		"Square"),
	RTTI_ENUM_VALUE(lx::ELfoShape::Pulse,		"Pulse"),
	RTTI_ENUM_VALUE(lx::ELfoShape::Gaussian,	"Gaussian")
RTTI_END_ENUM

RTTI_BEGIN_CLASS(lx::LfoModulator)
	RTTI_PROPERTY("Shape",			&lx::LfoModulator::mShape,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Frequency",		&lx::LfoModulator::mFrequency,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("PulseWidth",		&lx::LfoModulator::mPulseWidth,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("GaussianWidth",	&lx::LfoModulator::mGaussianWidth,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("PhaseOffset",	&lx::LfoModulator::mPhaseOffset,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Retrigger",		&lx::LfoModulator::mRetrigger,		nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace lx
{
	// ponytail: shapes computed directly (std::sin etc.) rather than math::waveform, to avoid any
	// range/convention mismatch — all outputs are explicitly 0..1.
	float LfoModulator::evaluate() const
	{
		constexpr float two_pi = 6.28318530718f;
		double raw = mElapsed * mFrequency + mPhaseOffset;
		float phase = static_cast<float>(raw - std::floor(raw));	// 0..1

		switch (mShape)
		{
		case ELfoShape::Sine:		return 0.5f + 0.5f * std::sin(two_pi * phase);
		case ELfoShape::Ramp:		return phase;
		case ELfoShape::Triangle:	return phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
		case ELfoShape::Square:		return phase < 0.5f ? 1.0f : 0.0f;
		case ELfoShape::Pulse:		return phase < mPulseWidth ? 1.0f : 0.0f;
		case ELfoShape::Gaussian:
		{
			float d = (phase - 0.5f) * mGaussianWidth;
			return std::exp(-(d * d));
		}
		default:					return 0.0f;
		}
	}


	void LfoModulator::onTrigger()
	{
		Modulator::onTrigger();
		if (mRetrigger)
			mElapsed = 0.0;	// restart the waveform from PhaseOffset
	}


	void LfoModulator::onStop()
	{
		// Free-running: no release ring-out.
		mHeld = false;
		mReleasing = false;
	}
}
