#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>

namespace lx
{
	enum class EControllerMode : int { Momentary, Toggle, FireOnly };

	/**
	 * A pure, reusable "what MIDI input is this" identity: a named Control mapped to MIDI (via one or
	 * more MidiBindings), with a firing Mode (Momentary: on-event fires, off-event stops; Toggle: each
	 * on-event toggles; FireOnly: each on-event re-fires). Decoupled from any single Trigger — which
	 * Trigger this Control fires is decided per-Program by a ControllerMapping (see controllermapping.h).
	 */
	class NAPAPI Controller : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		std::string					mName;						///< Property: 'Name'
		EControllerMode				mMode = EControllerMode::Momentary;	///< Property: 'Mode'

		// Runtime, non-serialized
		bool	mLatched = false;	///< Toggle mode latch
		bool	mHeld = false;		///< Momentary mode edge-detect
	};
}
