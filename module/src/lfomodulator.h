#pragma once

// Local Includes
#include "modulator.h"

namespace lx
{
	enum class ELfoShape : int { Sine, Ramp, Triangle, Square, Pulse, Gaussian };
	enum class ELfoMode  : int { Loop, OneShot, LoopRetrigger };

	/**
	 * Low-frequency oscillator as a fixed 1-second one-period curve; frequency is the player's playback
	 * speed (so changing Frequency needs no re-author, only a shape change does). Mode: Loop (free-running
	 * while held), OneShot (one period then stop), LoopRetrigger (loops; each trigger resets phase to 0).
	 * PhaseOffset seeds the initial player time. Ceiling: Sine/Gaussian are bezier approximations;
	 * Square/Pulse edges are a ~10ms ramp (napsequence curve segments are continuous, no true step).
	 */
	class NAPAPI LfoModulator : public Modulator
	{
		RTTI_ENABLE(Modulator)
	public:
		void generateCurve(nap::lxcontrolService& svc) override;
		void onTrigger() override;
		void onStop() override;
		void update(double deltaTime) override;

		ELfoShape	mShape = ELfoShape::Sine;	///< Property: 'Shape'
		ELfoMode	mMode = ELfoMode::Loop;		///< Property: 'Mode'
		float		mFrequency = 1.0f;			///< Property: 'Frequency' Hz
		float		mPulseWidth = 0.5f;			///< Property: 'PulseWidth' 0..1 (Pulse shape)
		float		mGaussianWidth = 8.0f;		///< Property: 'GaussianWidth' bell steepness
		float		mPhaseOffset = 0.0f;		///< Property: 'PhaseOffset' 0..1
	};
}
