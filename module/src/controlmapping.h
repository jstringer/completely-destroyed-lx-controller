#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>

// Local Includes
#include "control.h"
#include "trigger.h"

namespace lx
{
	/**
	 * Per-Program join: "in this Program, Control X fires Trigger Y." Decouples Control (a pure,
	 * reusable MIDI-mappable identity) from any single fixed Trigger, so the same Control can be
	 * mapped to a different Trigger in each Program. Owned by lxcontrolService, listed in each
	 * Program::mControlMappings.
	 */
	class NAPAPI ControlMapping : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		nap::ResourcePtr<Control>	mControl;	///< Property: 'Control'
		nap::ResourcePtr<Trigger>		mTrigger;		///< Property: 'Trigger'
	};
}
