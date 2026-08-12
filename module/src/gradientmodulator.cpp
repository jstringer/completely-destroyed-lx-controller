#include "gradientmodulator.h"
#include "lxcontrolservice.h"

#include <mathutils.h>
#include <cmath>

RTTI_BEGIN_CLASS(lx::GradientModulator)
	RTTI_PROPERTY("StartInput",	&lx::GradientModulator::mStartInput,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("EndInput",	&lx::GradientModulator::mEndInput,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("PhaseInput",	&lx::GradientModulator::mPhaseInput,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("PeriodInput",	&lx::GradientModulator::mPeriodInput,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace lx
{
	std::vector<PatchParameter*> GradientModulator::inputs()
	{
		return { mStartInput.get(), mEndInput.get(), mPhaseInput.get(), mPeriodInput.get() };
	}


	float GradientModulator::rawValue(float pos01, int component) const
	{
		const float phase  = mPhaseInput != nullptr ? mPhaseInput->mValue : 0.0f;
		const float period = inputValue(mPeriodInput, 0.1f, 4.0f);

		float p = std::fmod(pos01 / period + phase, 1.0f);
		if (p < 0.0f)
			p += 1.0f;
		// Mirror rather than hard-cut, so a Period < 1 repeats without a seam on the rig.
		const float ramp = p < 0.5f ? p * 2.0f : (1.0f - p) * 2.0f;

		const int c = nap::math::clamp(component, 0, 2);
		const float s = mStartInput != nullptr ? mStartInput->getBaseValue(c) : 0.0f;
		const float e = mEndInput != nullptr ? mEndInput->getBaseValue(c) : 1.0f;
		return s + (e - s) * ramp;
	}
}
