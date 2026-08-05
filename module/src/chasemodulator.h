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
	class NAPAPI ChaseModulator : public Modulator
	{
		RTTI_ENABLE(Modulator)
	public:
		void generateCurve(nap::lxcontrolService& svc) override;
		void onTrigger() override;
		void onStop() override;
		float value(float pos01, int component) const override;
		/** Endpoint-exclusive positions: with i/(n-1) the last source would sit at phase 1.0 == 0.0 and
		 *  duplicate source 0, collapsing an N-step chase into N-1 steps. */
		bool cyclicPositions() const override		{ return true; }

		float	mRate = 1.0f;		///< Property: 'Rate' Hz -- one full sweep across all sources per 1/Rate seconds
		float	mPulseWidth = 0.3f;	///< Property: 'PulseWidth' 0..1, fraction of each source's turn spent "on"
		bool	mGlide = false;		///< Property: 'Glide' soft edges instead of a hard on/off cut
	};
}
