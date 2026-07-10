#include "midibinding.h"

#include <algorithm>

RTTI_BEGIN_CLASS(lx::MidiBinding)
	RTTI_PROPERTY("Ports",			&lx::MidiBinding::mPorts,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Channels",		&lx::MidiBinding::mChannels,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Numbers",		&lx::MidiBinding::mNumbers,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("NoteOn",			&lx::MidiBinding::mNoteOn,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("NoteOff",		&lx::MidiBinding::mNoteOff,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Aftertouch",		&lx::MidiBinding::mAftertouch,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("ControlChange",	&lx::MidiBinding::mControlChange,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("ProgramChange",	&lx::MidiBinding::mProgramChange,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("ChannelPressure",	&lx::MidiBinding::mChannelPressure,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("PitchBend",		&lx::MidiBinding::mPitchBend,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Controller",		&lx::MidiBinding::mController,		nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace lx
{
	bool MidiBinding::matches(const nap::MidiEvent& event) const
	{
		bool type_ok = false;
		switch (event.getType())
		{
		case nap::MidiEvent::Type::noteOn:			type_ok = mNoteOn;			break;
		case nap::MidiEvent::Type::noteOff:			type_ok = mNoteOff;			break;
		case nap::MidiEvent::Type::afterTouch:		type_ok = mAftertouch;		break;
		case nap::MidiEvent::Type::controlChange:	type_ok = mControlChange;	break;
		case nap::MidiEvent::Type::programChange:	type_ok = mProgramChange;	break;
		case nap::MidiEvent::Type::channelPressure:	type_ok = mChannelPressure;	break;
		case nap::MidiEvent::Type::pitchBend:		type_ok = mPitchBend;		break;
		default:									type_ok = false;			break;
		}
		if (!type_ok)
			return false;

		if (!mNumbers.empty() && std::find(mNumbers.begin(), mNumbers.end(), static_cast<int>(event.getNumber())) == mNumbers.end())
			return false;
		if (!mChannels.empty() && std::find(mChannels.begin(), mChannels.end(), static_cast<int>(event.getChannel())) == mChannels.end())
			return false;
		if (!mPorts.empty() && std::find(mPorts.begin(), mPorts.end(), event.getPort()) == mPorts.end())
			return false;
		return true;
	}
}
