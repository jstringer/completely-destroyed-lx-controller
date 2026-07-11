#pragma once

// Local Includes
#include "modulator.h"
#include <vector>

namespace lx
{
	enum class EStepAdvance : int { Clock, Trigger };

	/**
	 * Step sequencer as a curve: each step value is held for 1/Rate seconds. No glide = stepped edges
	 * (~10ms ramp, like Square); Glide = linear ramps between step values. Loop repeats the pattern.
	 * Ceiling: Trigger-advance mode is not yet a per-trigger seek — it currently plays like Clock mode.
	 */
	class NAPAPI StepModulator : public Modulator
	{
		RTTI_ENABLE(Modulator)
	public:
		void generateCurve(nap::lxcontrolService& svc) override;
		void onTrigger() override;
		void onStop() override;

		std::vector<float>	mSteps;						///< Property: 'Steps' values 0..1
		float				mRate = 2.0f;				///< Property: 'Rate' steps/sec
		EStepAdvance		mAdvance = EStepAdvance::Clock;	///< Property: 'Advance'
		bool				mLoop = true;				///< Property: 'Loop'
		bool				mGlide = false;				///< Property: 'Glide'
	};
}
