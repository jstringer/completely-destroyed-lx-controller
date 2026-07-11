#include "stepmodulator.h"
#include "lxcontrolservice.h"

#include <sequenceplayer.h>
#include <algorithm>

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
	static constexpr double sEdge = 0.01;	// fake-vertical edge for stepped (non-glide) transitions

	void StepModulator::generateCurve(nap::lxcontrolService& svc)
	{
		using I = nap::math::ECurveInterp;
		std::vector<lx::Key> keys;

		if (mSteps.empty())
		{
			mDuration = 1.0;
			keys = { {0.0, 0.0f, I::Linear}, {1.0, 0.0f, I::Linear} };
			svc.authorFloatCurve(*mEditor, mTrackID, keys);
			return;
		}

		const double rate = std::max(mRate, 0.001f);
		const double step_dur = 1.0 / rate;
		const int n = (int)mSteps.size();
		mDuration = n * step_dur;

		if (mGlide)
		{
			// Linear ramp between successive step values (value at step i's start == steps[i]).
			for (int i = 0; i < n; ++i)
				keys.push_back({ i * step_dur, mSteps[i], I::Linear });
			keys.push_back({ n * step_dur, mSteps[0], I::Linear });	// glide back toward the loop point
		}
		else
		{
			// Flat hold per step with a fast edge to the next value.
			for (int i = 0; i < n; ++i)
			{
				double t0 = i * step_dur;
				double t1 = (i + 1) * step_dur;
				keys.push_back({ t0, mSteps[i], I::Linear });
				keys.push_back({ t1 - sEdge, mSteps[i], I::Linear });
			}
			keys.push_back({ n * step_dur, mSteps[n - 1], I::Linear });
		}
		svc.authorFloatCurve(*mEditor, mTrackID, keys);
	}


	void StepModulator::onTrigger()
	{
		Modulator::onTrigger();
		if (mPlayer == nullptr)
			return;
		// ceiling: Trigger-advance not yet a per-trigger seek; both modes play on the clock for now.
		mPlayer->setIsLooping(mLoop);
		mPlayer->setPlayerTime(0.0);
		mPlayer->setIsPlaying(true);
	}


	void StepModulator::onStop()
	{
		Modulator::onStop();
		if (mPlayer != nullptr)
			mPlayer->setIsPlaying(false);
	}
}
