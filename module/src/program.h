#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <vector>

// Local Includes
#include "trigger.h"

namespace lx
{
	/**
	 * A named set of Triggers that are live together. Loading a program fires its EnterTriggers and
	 * arms its ControllerTriggers (only its ControllerTriggers respond to MIDI); unloading fires its
	 * ExitTriggers and stops the rest.
	 */
	class NAPAPI Program : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		std::string							mName;		///< Property: 'Name'
		std::vector<nap::ResourcePtr<Trigger>>	mTriggers;	///< Property: 'Triggers'
	};
}
