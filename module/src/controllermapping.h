#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>

// Local Includes
#include "controller.h"
#include "trigger.h"

namespace lx
{
	/**
	 * Per-Program join: "in this Program, Control X fires Trigger Y." Decouples Controller (a pure,
	 * reusable MIDI-mappable identity) from any single fixed Trigger, so the same Controller can be
	 * mapped to a different Trigger in each Program. Owned by lxcontrolService, listed in each
	 * Program::mControllerMappings.
	 */
	class NAPAPI ControllerMapping : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		nap::ResourcePtr<Controller>	mController;	///< Property: 'Controller'
		nap::ResourcePtr<Trigger>		mTrigger;		///< Property: 'Trigger'
	};
}
