#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <vector>
#include <string>

// Local Includes
#include "fixturegroup.h"
#include "patch.h"

namespace lx
{
	/**
	 * Binds a Patch to one or more FixtureGroups. When the owning Trigger fires, the patch is triggered and
	 * claims the matching channels of every source in those groups -- one source per fixture for
	 * Dimmer/Strobe-type roles, one per colour unit for RGB roles (see lxcontrolService::sourcesFor).
	 */
	struct NAPAPI PatchFixtureBinding
	{
		nap::ResourcePtr<Patch>								mPatch;		///< Property: 'Patch'
		std::vector<nap::ResourcePtr<FixtureGroup>>			mGroups;	///< Property: 'Groups'
		/// Property: 'EndToEnd' -- with 2+ groups: false (default) spreads each group across 0..1 on its
		/// own, so they run in parallel; true concatenates them into one run so the effect crosses the
		/// whole set once.
		bool												mEndToEnd = false;
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
