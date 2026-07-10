#pragma once

// Local Includes
#include "modulator.h"

namespace lx
{
	/**
	 * Attack-Decay-Sustain-Release envelope, computed procedurally over the player clock. Sustain is
	 * held exactly while triggered (no pause/seek), release rings out after onStop().
	 */
	class NAPAPI AdsrModulator : public Modulator
	{
		RTTI_ENABLE(Modulator)
	public:
		float evaluate() const override;
		void onTrigger() override;
		bool isFinished() const override;

		float	mAttack = 0.01f;	///< Property: 'Attack' seconds
		float	mDecay = 0.1f;		///< Property: 'Decay' seconds
		float	mSustain = 0.7f;	///< Property: 'Sustain' 0..1
		float	mRelease = 0.5f;	///< Property: 'Release' seconds
		bool	mLoop = false;		///< Property: 'Loop'
	};
}
