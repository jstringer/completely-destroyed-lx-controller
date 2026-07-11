#pragma once

// Local Includes
#include "modulator.h"

namespace lx
{
	/**
	 * Attack-Decay-Sustain-Release envelope as a curve: points (0,0)->(A,1)->(A+D,S)->(A+D+R,0).
	 * Sustain is held by pausing the player at t=A+D (paused adapters keep sampling the frozen time, so
	 * the sink holds the sustain level); gate-off jumps to A+D and plays the release out. With Loop the
	 * whole envelope repeats while held instead of sustaining.
	 * Ceiling: release always starts from the sustain level (a gate-off during attack/decay snaps to it).
	 */
	class NAPAPI AdsrModulator : public Modulator
	{
		RTTI_ENABLE(Modulator)
	public:
		void generateCurve(nap::lxcontrolService& svc) override;
		void onTrigger() override;
		void onStop() override;
		void update(double deltaTime) override;
		bool isFinished() const override;

		float	mAttack = 0.01f;	///< Property: 'Attack' seconds
		float	mDecay = 0.1f;		///< Property: 'Decay' seconds
		float	mSustain = 0.7f;	///< Property: 'Sustain' 0..1
		float	mRelease = 0.5f;	///< Property: 'Release' seconds
		bool	mLoop = false;		///< Property: 'Loop'

	private:
		double	mSustainTime = 0.0;	///< t = attack + decay (where playback pauses to hold sustain)
	};
}
