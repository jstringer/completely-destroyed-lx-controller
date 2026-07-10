#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <midievent.h>
#include <vector>
#include <string>

// Local Includes
#include "controller.h"

namespace lx
{
	/**
	 * A MIDI message filter (mirrors MidiInputComponent's) plus the Controller it drives. Many bindings
	 * can point at one Controller. Empty Ports/Channels/Numbers act as wildcards.
	 */
	class NAPAPI MidiBinding : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		/** @return true if the event passes this filter. */
		bool matches(const nap::MidiEvent& event) const;

		std::vector<std::string>	mPorts;			///< Property: 'Ports'
		std::vector<int>			mChannels;		///< Property: 'Channels'
		std::vector<int>			mNumbers;		///< Property: 'Numbers'
		bool	mNoteOn = false;			///< Property: 'NoteOn'
		bool	mNoteOff = false;			///< Property: 'NoteOff'
		bool	mAftertouch = false;		///< Property: 'Aftertouch'
		bool	mControlChange = false;		///< Property: 'ControlChange'
		bool	mProgramChange = false;		///< Property: 'ProgramChange'
		bool	mChannelPressure = false;	///< Property: 'ChannelPressure'
		bool	mPitchBend = false;			///< Property: 'PitchBend'

		nap::ResourcePtr<Controller>	mController;	///< Property: 'Controller'
	};
}
