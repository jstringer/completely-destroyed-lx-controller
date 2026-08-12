#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <midievent.h>
#include <vector>
#include <string>

// Local Includes
#include "control.h"

namespace lx
{
	/**
	 * A MIDI message filter plus the Control it drives. Many bindings can point at one Control.
	 *
	 * Deliberately device-AGNOSTIC: message type + number only, never port or channel (see
	 * lxcontrolService::createBinding). A pad learned on one controller then fires from any device
	 * sending the same note, which is what a touring desk wants; filtering on port/channel would break
	 * every learned binding the moment a device re-enumerates. An empty Numbers list is a wildcard.
	 */
	class NAPAPI MidiBinding : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		/** @return true if the event passes this filter. */
		bool matches(const nap::MidiEvent& event) const;

		std::vector<int>			mNumbers;		///< Property: 'Numbers'
		bool	mNoteOn = false;			///< Property: 'NoteOn'
		bool	mNoteOff = false;			///< Property: 'NoteOff'
		bool	mAftertouch = false;		///< Property: 'Aftertouch'
		bool	mControlChange = false;		///< Property: 'ControlChange'
		bool	mProgramChange = false;		///< Property: 'ProgramChange'
		bool	mChannelPressure = false;	///< Property: 'ChannelPressure'
		bool	mPitchBend = false;			///< Property: 'PitchBend'

		nap::ResourcePtr<Control>	mControl;	///< Property: 'Control'
	};
}
