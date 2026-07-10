#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>

// Local Includes
#include "trigger.h"

namespace lx
{
	enum class EControllerMode : int { Momentary, Toggle, FireOnly };

	/**
	 * Maps incoming MIDI (via one or more MidiBindings) to firing/stopping a single Trigger, per Mode:
	 * Momentary (on-event fires, off-event stops), Toggle (each on-event toggles), FireOnly (each
	 * on-event re-fires).
	 */
	class NAPAPI Controller : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		std::string					mName;						///< Property: 'Name'
		nap::ResourcePtr<Trigger>	mTrigger;					///< Property: 'Trigger'
		EControllerMode				mMode = EControllerMode::Momentary;	///< Property: 'Mode'

		// Runtime, non-serialized
		bool	mLatched = false;	///< Toggle mode latch
		bool	mHeld = false;		///< Momentary mode edge-detect
	};
}
