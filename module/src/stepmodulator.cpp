#include "stepmodulator.h"

#include <cmath>

RTTI_BEGIN_ENUM(lx::EStepAdvance)
	RTTI_ENUM_VALUE(lx::EStepAdvance::Clock,	"Clock"),
	RTTI_ENUM_VALUE(lx::EStepAdvance::Trigger,	"Trigger")
RTTI_END_ENUM

RTTI_BEGIN_CLASS(lx::StepModulator)
	RTTI_PROPERTY("Steps",		&lx::StepModulator::mSteps,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Rate",		&lx::StepModulator::mRate,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Advance",	&lx::StepModulator::mAdvance,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Loop",		&lx::StepModulator::mLoop,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Glide",		&lx::StepModulator::mGlide,		nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace lx
{
	float StepModulator::evaluate(double time)
	{
		if (mSteps.empty())
			return 0.0f;

		int count = static_cast<int>(mSteps.size());

		if (mAdvance == EStepAdvance::Trigger)
		{
			int idx = mLoop ? (mStepIndex % count) : std::min(mStepIndex, count - 1);
			return mSteps[idx];
		}

		// Clock mode
		double pos = time * mRate;
		double base = std::floor(pos);
		int idx = static_cast<int>(base);
		if (mLoop)
			idx = ((idx % count) + count) % count;
		else
			idx = std::min(std::max(idx, 0), count - 1);

		if (!mGlide)
			return mSteps[idx];

		int next = mLoop ? (idx + 1) % count : std::min(idx + 1, count - 1);
		float frac = static_cast<float>(pos - base);
		return mSteps[idx] + (mSteps[next] - mSteps[idx]) * frac;
	}


	void StepModulator::onTrigger()
	{
		Modulator::onTrigger();
		if (mAdvance == EStepAdvance::Trigger)
			mStepIndex++;
	}


	void StepModulator::onStop()
	{
		mHeld = false;
		mReleasing = false;
	}
}
