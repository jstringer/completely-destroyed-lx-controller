#pragma once

// Local Includes
#include "modulator.h"

namespace lx
{
	enum class ELfoShape : int { Sine, Ramp, Triangle, Square, Pulse, Gaussian };

	/**
	 * Low-frequency oscillator. Free-running over the player clock; evaluate() samples the shape at
	 * phase = frac(time * Frequency + PhaseOffset). Retrigger resets the (own) player to phase 0.
	 */
	class NAPAPI LfoModulator : public Modulator
	{
		RTTI_ENABLE(Modulator)
	public:
		float evaluate() const override;
		void onTrigger() override;
		void onStop() override;

		ELfoShape	mShape = ELfoShape::Sine;	///< Property: 'Shape'
		float		mFrequency = 1.0f;			///< Property: 'Frequency' Hz
		float		mPulseWidth = 0.5f;			///< Property: 'PulseWidth' 0..1 (Pulse shape)
		float		mGaussianWidth = 8.0f;		///< Property: 'GaussianWidth' bell steepness
		float		mPhaseOffset = 0.0f;		///< Property: 'PhaseOffset' 0..1
		bool		mRetrigger = false;			///< Property: 'Retrigger' reset phase on trigger
	};
}
