#pragma once

// External Includes
#include <algorithm>

// Local Includes
#include "modulator.h"

namespace lx
{
	/**
	 * Sample-and-hold noise across the source list -- each source independently steps to a new
	 * pseudo-random value at Rate Hz (optionally smoothed between steps). Uses its player purely as a
	 * shared phase clock (like ChaseModulator); the value comes from a small deterministic integer hash of
	 * (position bucket, step index, component), not from the sink/curve (generateCurve() authors a flat
	 * dummy curve solely to pin mDuration/the sequence duration). Deterministic: no persisted RNG state.
	 *
	 * Hashing on `component` is what lets ONE Noise on a Colour parameter decorrelate R/G/B -- the job the
	 * old per-instance Seed property did by hand, which is why that property is gone.
	 */
	class NAPAPI NoiseModulator : public Modulator
	{
		RTTI_ENABLE(Modulator)
	public:
		void generateCurve(nap::lxcontrolService& svc) override;
		void onTrigger() override;
		void onStop() override;
		void update(double deltaTime) override;
		float value(float pos01, int component) const override;

		float	mRate = 2.0f;		///< Property: 'Rate' Hz -- new random value per source this many times/sec
		float	mSmoothing = 0.5f;	///< Property: 'Smoothing' 0 = hard sample-and-hold steps, >0 = eased between steps

	private:
		// Self-accumulated elapsed time, NOT read from mPlayer->getPlayerTime(): the player loops every
		// `mDuration` (1s, from the dummy curve) which would make the noise pattern repeat every second.
		// mPlayer is still built/played only to gate isFinished()/onTrigger/onStop the same way every
		// other modulator does.
		double	mElapsed = 0.0;
	};
}
