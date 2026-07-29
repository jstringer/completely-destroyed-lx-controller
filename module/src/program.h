#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <vector>

// Local Includes
#include "trigger.h"
#include "controlmapping.h"

namespace lx
{
	/**
	 * A named set of lifecycle Triggers (fired/stopped automatically on load/unload) plus a per-Control
	 * routing table (ControlMapping) that says which Trigger each Control fires while this
	 * Program is active. Loading a program fires its Enter triggers; unloading fires its Exit triggers
	 * and stops the rest. Control-kind triggers respond to MIDI only via a ControlMapping in
	 * mControlMappings, and only while this program is active.
	 */
	class NAPAPI Program : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		std::string											mName;					///< Property: 'Name'
		std::vector<nap::ResourcePtr<Trigger>>				mLifecycleTriggers;		///< Property: 'LifecycleTriggers' (Enter/Exit only)
		std::vector<nap::ResourcePtr<ControlMapping>>	mControlMappings;	///< Property: 'ControlMappings'
	};
}
