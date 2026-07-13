#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <vector>

// Local Includes
#include "effectparameter.h"
#include "modulator.h"

namespace lx
{
	/** Whether an Effect drives one shared value (Single) or a distinct value per fixture slot (Multiple). */
	enum class EEffectTargetMode : int { Single, Multiple };

	/**
	 * A named bundle of EffectParameters and Modulators. Has no fixture knowledge: trigger()/stop()
	 * forward to modulators; update() resets each parameter to its base then blends every modulator's
	 * value into its target component(s) per its Blend. The result (EffectParameter::mCurrentValues)
	 * is what the LTP claim stack (Phase 3) reads.
	 *
	 * TargetMode/FixtureCount declare how many independent fixture "slots" this effect computes: Single
	 * (default) is one shared value, broadcast to every fixture a Trigger binds it to; Multiple computes
	 * FixtureCount distinct slot values (see Modulator::valueForSlot), one per bound fixture, assigned in
	 * physical rig order by lxcontrolService::fireTrigger().
	 */
	class NAPAPI Effect : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		void trigger();
		void stop();
		void update(double deltaTime);
		bool isFinished() const;

		std::string								mName;			///< Property: 'Name'
		std::vector<nap::ResourcePtr<EffectParameter>>	mParameters;	///< Property: 'Parameters'
		std::vector<nap::ResourcePtr<Modulator>>		mModulators;	///< Property: 'Modulators'
		EEffectTargetMode						mTargetMode = EEffectTargetMode::Single;	///< Property: 'TargetMode'
		int										mFixtureCount = 2;		///< Property: 'FixtureCount' (Multiple only)
	};
}
