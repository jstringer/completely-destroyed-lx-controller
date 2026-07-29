#pragma once

// External Includes
#include <algorithm>

// Local Includes
#include "modulator.h"

namespace lx
{
	/**
	 * Decorrelated sample-and-hold noise per fixture voice -- each voice independently steps to a new
	 * pseudo-random value at Rate Hz (optionally smoothed between steps). Uses its player purely as a
	 * shared phase clock (like ChaseModulator); the actual per-voice value comes from a small deterministic
	 * integer hash of (voice, step index), not from the sink/curve (generateCurve() authors a flat dummy
	 * curve solely to pin mDuration/the sequence duration). Deterministic: no persisted RNG state needed.
	 * Only meaningful on a Multiple-mode Patch (see Patch::mTargetMode); on a Single-mode patch it
	 * degenerates to a single noise generator on voice 0.
	 */
	class NAPAPI NoiseModulator : public Modulator
	{
		RTTI_ENABLE(Modulator)
	public:
		void generateCurve(nap::lxcontrolService& svc) override;
		void onTrigger() override;
		void onStop() override;
		void update(double deltaTime) override;
		float valueForVoice(int voice) const override;
		void setVoiceCount(int count) override		{ mVoiceCount = std::max(1, count); }

		float	mRate = 2.0f;		///< Property: 'Rate' Hz -- new random value per voice this many times/sec
		float	mSmoothing = 0.5f;	///< Property: 'Smoothing' 0 = hard sample-and-hold steps, >0 = eased between steps
		int		mSeed = 0;			///< Property: 'Seed' -- salts the per-(voice,step) hash so two independent
									///< NoiseModulators (e.g. one per R/G/B component of a color) decorrelate
									///< instead of producing identical values. lxcontrolService::addModulator
									///< auto-assigns a fresh seed to each newly-created NoiseModulator.

	private:
		int		mVoiceCount = 1;	///< Runtime: set by lxcontrolService::setPatchTargetMode / rebuildFromLoadedContent
		// Self-accumulated elapsed time, NOT read from mPlayer->getPlayerTime(): the player loops every
		// `mDuration` (1s, from the dummy curve) which would make the noise pattern repeat every second.
		// mPlayer is still built/played only to gate isFinished()/onTrigger/onStop the same way every
		// other modulator does.
		double	mElapsed = 0.0;
	};
}
