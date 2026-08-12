#pragma once

// Local Includes
#include "modulator.h"
#include <vector>

namespace lx
{
	/**
	 * Step sequencer as a curve: each step value is held for 1/Rate seconds. No glide = stepped edges
	 * (~10ms ramp, like Square); Glide = linear ramps between step values. Loop repeats the pattern.
	 */
	class NAPAPI StepModulator : public Modulator
	{
		RTTI_ENABLE(Modulator)
	public:
		void generateCurve(nap::lxcontrolService& svc) override;
		void onTrigger() override;
		void onStop() override;

		std::vector<float>	mSteps;			///< Property: 'Steps' values 0..1
		float				mRate = 2.0f;	///< Property: 'Rate' steps/sec
		bool				mLoop = true;	///< Property: 'Loop'
		bool				mGlide = false;	///< Property: 'Glide'
	};
}
