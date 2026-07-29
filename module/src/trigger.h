#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <vector>
#include <string>

// Local Includes
#include "patch.h"

namespace lx
{
	/**
	 * Binds a Patch to a set of fixtures (by entity mID). When the owning Trigger fires, the patch
	 * is triggered and claims the matching channels on each named fixture.
	 */
	struct NAPAPI PatchFixtureBinding
	{
		nap::ResourcePtr<Patch>	mPatch;			///< Property: 'Patch'
		std::vector<std::string>	mFixtureNames;		///< Property: 'Fixtures' fixture entity mIDs
	};


	/** How a Trigger fires: Control = by a MIDI-mapped Control; Enter/Exit = by its Program loading/unloading. */
	enum class ETriggerKind : int { Control, Enter, Exit };


	/**
	 * A named set of patch->fixture bindings that can be fired/stopped. Firing is arbitrated by
	 * lxcontrolService (which owns activation ids + the LTP claim stack). Kind only affects how it is
	 * fired: Control by a MIDI-mapped Control, Enter/Exit by Program load/unload.
	 */
	class NAPAPI Trigger : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		std::string							mName;			///< Property: 'Name'
		ETriggerKind						mKind = ETriggerKind::Control;	///< Property: 'Kind'
		std::vector<PatchFixtureBinding>	mBindings;		///< Property: 'Bindings'
	};
}
