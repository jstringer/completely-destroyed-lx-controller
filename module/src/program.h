#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <vector>

// Local Includes
#include "trigger.h"
#include "controllermapping.h"

namespace lx
{
	/**
	 * A named set of lifecycle Triggers (fired/stopped automatically on load/unload) plus a per-Control
	 * routing table (ControllerMapping) that says which Trigger each Controller fires while this
	 * Program is active. Loading a program fires its EnterTriggers; unloading fires its ExitTriggers
	 * and stops the rest. ControllerTriggers respond to MIDI only via a ControllerMapping in
	 * mControllerMappings, and only while this program is active.
	 */
	class NAPAPI Program : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		std::string											mName;					///< Property: 'Name'
		std::vector<nap::ResourcePtr<Trigger>>				mLifecycleTriggers;		///< Property: 'LifecycleTriggers' (Enter/Exit only)
		std::vector<nap::ResourcePtr<ControllerMapping>>	mControllerMappings;	///< Property: 'ControllerMappings'
	};
}
