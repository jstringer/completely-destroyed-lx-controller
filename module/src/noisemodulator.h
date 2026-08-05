#pragma once

// External Includes
#include <algorithm>

// Local Includes
#include "modulator.h"

namespace lx
{
	/**
	 * Value noise across the source list AND over time -- bilinear over (position * Density, time * Rate),
	 * from a small deterministic integer hash. Density is the spatial frequency: low values make the whole
	 * rig drift together, high values decorrelate neighbours the way the old per-voice hash always did.
	 * Uses its player purely as a gate (like ChaseModulator); generateCurve() authors a flat dummy curve
	 * solely to pin mDuration/the sequence duration. Deterministic: no persisted RNG state.
	 *
	 * Hashing on `component` is what lets ONE Noise on a Colour parameter decorrelate R/G/B -- the job the
	 * old per-instance Seed property did by hand, which is why that property is gone.
	 */
	class NAPAPI NoiseModulator : public FieldModulator
	{
		RTTI_ENABLE(FieldModulator)
	public:
		void generateCurve(nap::lxcontrolService& svc) override;
		void onTrigger() override;
		void onStop() override;
		void update(double deltaTime) override;
		float rawValue(float pos01, int component) const override;
		std::vector<PatchParameter*> inputs() override
			{ return { mRateInput.get(), mDensityInput.get(), mSmoothingInput.get() }; }

		nap::ResourcePtr<FloatParameter>	mRateInput;			///< Property: 'RateInput' 0..1 -> [0.05, 8] Hz (time)
		/// Property: 'DensityInput' 0..1 -> [1, 24] cells across the source list. Low = the whole rig drifts
		/// together as one wash; high = neighbouring sources differ sharply (the old per-voice look).
		nap::ResourcePtr<FloatParameter>	mDensityInput;
		nap::ResourcePtr<FloatParameter>	mSmoothingInput;	///< Property: 'SmoothingInput' 0 = hard sample-and-hold, 1 = eased

	private:
		// Self-accumulated elapsed time, NOT read from mPlayer->getPlayerTime(): the player loops every
		// `mDuration` (1s, from the dummy curve) which would make the noise pattern repeat every second.
		// mPlayer is still built/played only to gate isFinished()/onTrigger/onStop the same way every
		// other modulator does.
		double	mElapsed = 0.0;
	};
}
