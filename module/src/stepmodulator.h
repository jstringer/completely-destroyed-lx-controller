#pragma once

// Local Includes
#include "modulator.h"
#include <vector>

namespace lx
{
	enum class EStepAdvance : int { Clock, Trigger };

	/**
	 * Step sequencer. In Clock mode it advances with the player clock (index = floor(time * Rate)); in
	 * Trigger mode each onTrigger() advances one step. Glide lerps between adjacent steps.
	 */
	class NAPAPI StepModulator : public Modulator
	{
		RTTI_ENABLE(Modulator)
	public:
		float evaluate() const override;
		void onTrigger() override;
		void onStop() override;

		std::vector<float>	mSteps;						///< Property: 'Steps' values 0..1
		float				mRate = 2.0f;				///< Property: 'Rate' steps/sec (Clock mode)
		EStepAdvance		mAdvance = EStepAdvance::Clock;	///< Property: 'Advance'
		bool				mLoop = true;				///< Property: 'Loop'
		bool				mGlide = false;				///< Property: 'Glide'

	private:
		int	mStepIndex = 0;	///< Trigger-mode current step (non-serialized)
	};
}
