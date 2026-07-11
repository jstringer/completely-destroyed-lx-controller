#pragma once

// Local Includes
#include "modulator.h"

namespace lx
{
	enum class EAdMode : int { OneShot, LoopWhileSustained };

	/**
	 * Attack-Decay envelope (no sustain level): curve (0,0)->(A,1)->(A+D,0). OneShot fires once per
	 * trigger; LoopWhileSustained repeats A->D while the gate is held and stops at the end of the current
	 * cycle on gate-off.
	 */
	class NAPAPI AdModulator : public Modulator
	{
		RTTI_ENABLE(Modulator)
	public:
		void generateCurve(nap::lxcontrolService& svc) override;
		void onTrigger() override;
		void onStop() override;
		void update(double deltaTime) override;

		float	mAttack = 0.05f;	///< Property: 'Attack' seconds
		float	mDecay = 0.25f;		///< Property: 'Decay' seconds
		EAdMode	mMode = EAdMode::OneShot;	///< Property: 'Mode'
	};
}
