#include "midimapping.h"

#include <algorithm>

RTTI_BEGIN_ENUM(nap::EMidiMappingTargetKind)
	RTTI_ENUM_VALUE(nap::EMidiMappingTargetKind::Parameter,		"Parameter"),
	RTTI_ENUM_VALUE(nap::EMidiMappingTargetKind::Preset,			"Preset"),
	RTTI_ENUM_VALUE(nap::EMidiMappingTargetKind::EffectTrigger,	"EffectTrigger")
RTTI_END_ENUM

RTTI_BEGIN_CLASS(nap::MidiMapping)
	RTTI_PROPERTY("Ports",				&nap::MidiMapping::mPorts,				nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Channels",			&nap::MidiMapping::mChannels,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Numbers",			&nap::MidiMapping::mNumbers,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("NoteOn",				&nap::MidiMapping::mNoteOn,				nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("NoteOff",			&nap::MidiMapping::mNoteOff,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Aftertouch",			&nap::MidiMapping::mAftertouch,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("ControlChange",		&nap::MidiMapping::mControlChange,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("ProgramChange",		&nap::MidiMapping::mProgramChange,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("ChannelPressure",		&nap::MidiMapping::mChannelPressure,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("PitchBend",			&nap::MidiMapping::mPitchBend,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("TargetKind",			&nap::MidiMapping::mTargetKind,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Parameter",			&nap::MidiMapping::mParameter,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("InputMinimum",		&nap::MidiMapping::mInputMinimum,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("InputMaximum",		&nap::MidiMapping::mInputMaximum,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Preset",				&nap::MidiMapping::mPreset,				nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("EffectLayer",		&nap::MidiMapping::mEffectLayer,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Action",				&nap::MidiMapping::mAction,				nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace nap
{
	bool MidiMapping::init(utility::ErrorState& errorState)
	{
		switch (mTargetKind)
		{
		case EMidiMappingTargetKind::Parameter:
			if (!errorState.check(mParameter != nullptr, "%s: TargetKind is Parameter but Parameter is not set", mID.c_str()))
				return false;
			break;
		case EMidiMappingTargetKind::Preset:
			if (!errorState.check(mPreset != nullptr, "%s: TargetKind is Preset but Preset is not set", mID.c_str()))
				return false;
			break;
		case EMidiMappingTargetKind::EffectTrigger:
			if (!errorState.check(mEffectLayer != nullptr, "%s: TargetKind is EffectTrigger but EffectLayer is not set", mID.c_str()))
				return false;
			break;
		}
		return true;
	}


	bool MidiMapping::matches(const MidiEvent& event) const
	{
		switch (event.getType())
		{
		case MidiEvent::Type::noteOn:			if (!mNoteOn) return false;				break;
		case MidiEvent::Type::noteOff:			if (!mNoteOff) return false;				break;
		case MidiEvent::Type::afterTouch:		if (!mAftertouch) return false;			break;
		case MidiEvent::Type::controlChange:	if (!mControlChange) return false;			break;
		case MidiEvent::Type::programChange:	if (!mProgramChange) return false;			break;
		case MidiEvent::Type::channelPressure:	if (!mChannelPressure) return false;		break;
		case MidiEvent::Type::pitchBend:		if (!mPitchBend) return false;				break;
		default:
			return false;
		}

		if (!mPorts.empty() && std::find(mPorts.begin(), mPorts.end(), event.getPort()) == mPorts.end())
			return false;

		if (!mChannels.empty() && std::find(mChannels.begin(), mChannels.end(), event.getChannel()) == mChannels.end())
			return false;

		if (!mNumbers.empty() && std::find(mNumbers.begin(), mNumbers.end(), event.getNumber()) == mNumbers.end())
			return false;

		return true;
	}
}
