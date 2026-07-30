#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>

namespace lx
{
	enum class EControlMode : int { Momentary, Toggle, FireOnly };
	enum class EControlKind : int { Pad, Knob };	///< presentational: pad (note/gate) vs knob (continuous)

	/**
	 * A pure, reusable "what MIDI input is this" identity: a named Control mapped to MIDI (via one or
	 * more MidiBindings), with a firing Mode (Momentary: on-event fires, off-event stops; Toggle: each
	 * on-event toggles; FireOnly: each on-event re-fires). Decoupled from any single Trigger — which
	 * Trigger this Control fires is decided per-Program by a ControlMapping (see controlmapping.h).
	 */
	class NAPAPI Control : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		std::string					mName;						///< Property: 'Name'
		EControlMode				mMode = EControlMode::Momentary;	///< Property: 'Mode'
		EControlKind				mKind = EControlKind::Pad;	///< Property: 'Kind' pad vs knob; presentational (chip in the CONTROLS UI).
		std::string					mGroup;						///< Property: 'Group' device label that groups Controls in the CONTROLS UI (empty = ungrouped). Presentational only; not a resource (findings §6).

		// Runtime, non-serialized
		bool	mLatched = false;	///< Toggle mode latch
		bool	mHeld = false;		///< Momentary mode edge-detect
	};
}
