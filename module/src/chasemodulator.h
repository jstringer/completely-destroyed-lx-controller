#pragma once

// External Includes
#include <algorithm>

// Local Includes
#include "modulator.h"

namespace lx
{
	/**
	 * Sweeps a one-hot pulse across FixtureCount slots in sequence -- "lights flash one after another".
	 * Uses its player purely as a shared phase clock (Rate -> playback speed, like LfoModulator); the
	 * actual per-slot value is computed analytically in valueForSlot() from the player's own time, not
	 * read back from the sink/curve (generateCurve() authors a flat dummy curve solely to pin mDuration/
	 * the sequence's playable duration to 1 second, matching LfoModulator's convention). Only meaningful
	 * on a Multiple-mode Effect (see Effect::mTargetMode); on a Single-mode effect it degenerates to a
	 * single repeating pulse on slot 0.
	 */
	class NAPAPI ChaseModulator : public Modulator
	{
		RTTI_ENABLE(Modulator)
	public:
		void generateCurve(nap::lxcontrolService& svc) override;
		void onTrigger() override;
		void onStop() override;
		float valueForSlot(int slot) const override;
		void setSlotCount(int count) override		{ mSlotCount = std::max(1, count); }

		float	mRate = 1.0f;		///< Property: 'Rate' Hz -- one full sweep across all slots per 1/Rate seconds
		float	mPulseWidth = 0.3f;	///< Property: 'PulseWidth' 0..1, fraction of each slot's turn spent "on"
		bool	mGlide = false;		///< Property: 'Glide' soft edges instead of a hard on/off cut

	private:
		int		mSlotCount = 1;	///< Runtime: set by lxcontrolService::setEffectTargetMode / rebuildFromLoadedContent
	};
}
