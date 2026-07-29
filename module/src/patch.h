#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <vector>

// Local Includes
#include "patchparameter.h"
#include "modulator.h"

namespace lx
{
	/** Whether an Patch drives one shared value (Single) or a distinct value per fixture voice (Multiple). */
	enum class EPatchTargetMode : int { Single, Multiple };

	/**
	 * A named bundle of PatchParameters and Modulators. Has no fixture knowledge: trigger()/stop()
	 * forward to modulators; update() resets each parameter to its base then blends every modulator's
	 * value into its target component(s) per its Blend. The result (PatchParameter::mCurrentValues)
	 * is what the LTP claim stack (Phase 3) reads.
	 *
	 * TargetMode/FixtureCount declare how many independent fixture "voices" this patch computes: Single
	 * (default) is one shared value, broadcast to every fixture a Trigger binds it to; Multiple computes
	 * FixtureCount distinct voice values (see Modulator::valueForVoice), one per bound fixture, assigned in
	 * physical rig order by lxcontrolService::fireTrigger().
	 */
	class NAPAPI Patch : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		void trigger();
		void stop();
		void update(double deltaTime);
		bool isFinished() const;

		std::string								mName;			///< Property: 'Name'
		std::vector<nap::ResourcePtr<PatchParameter>>	mParameters;	///< Property: 'Parameters'
		std::vector<nap::ResourcePtr<Modulator>>		mModulators;	///< Property: 'Modulators'
		EPatchTargetMode						mTargetMode = EPatchTargetMode::Single;	///< Property: 'TargetMode'
		/// Property: 'FixtureCount' (Multiple only). No longer hand-authored: lxcontrolService derives and
		/// overwrites this from the actual Trigger binding whenever the patch fires (see
		/// lxcontrolService::syncPatchFixtureCount). The default of 1 only matters for a brand-new
		/// Multiple-mode patch that hasn't fired yet (e.g. the Patches tab's per-voice preview).
		int										mFixtureCount = 1;
	};
}
