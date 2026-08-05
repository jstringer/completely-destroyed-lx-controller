#pragma once

// External Includes
#include <algorithm>

// Local Includes
#include "modulator.h"

namespace lx
{
	/**
	 * Sweeps a one-hot pulse along the source list in sequence -- "lights flash one after another".
	 * Uses its player purely as a shared phase clock (Rate -> playback speed, like LfoModulator); the
	 * value is computed analytically in value() from the player's own time and the source's position, not
	 * read back from the sink/curve (generateCurve() authors a flat dummy curve solely to pin mDuration/
	 * the sequence's playable duration to 1 second, matching LfoModulator's convention). A single-source
	 * spread degenerates to one repeating pulse, which is what "Single" mode used to mean.
	 */
	class NAPAPI ChaseModulator : public FieldModulator
	{
		RTTI_ENABLE(FieldModulator)
	public:
		void generateCurve(nap::lxcontrolService& svc) override;
		void onTrigger() override;
		void onStop() override;
		float value(float pos01, int component) const override;
		std::vector<PatchParameter*> inputs() override
			{ return { mRateInput.get(), mPulseWidthInput.get() }; }
		/** Endpoint-exclusive positions: with i/(n-1) the last source would sit at phase 1.0 == 0.0 and
		 *  duplicate source 0, collapsing an N-step chase into N-1 steps. */
		bool cyclicPositions() const override		{ return true; }

		nap::ResourcePtr<FloatParameter>	mRateInput;			///< Property: 'RateInput' 0..1 -> [0.05, 8] Hz
		nap::ResourcePtr<FloatParameter>	mPulseWidthInput;	///< Property: 'PulseWidthInput' 0..1 -> [0.01, 1]
		bool	mGlide = false;		///< Property: 'Glide' soft edges instead of a hard on/off cut
	};
}
