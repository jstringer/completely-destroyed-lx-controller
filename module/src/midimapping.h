#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <midievent.h>
#include <parameternumeric.h>

// Local Includes
#include "preset.h"
#include "effectlayer.h"
#include "miditriggercomponent.h"

namespace nap
{
	/**
	 * What a MidiMapping drives when its filter matches an incoming MidiEvent.
	 */
	enum class EMidiMappingTargetKind : int
	{
		Parameter		= 0,	///< Drive a Parameter's value from the message's normalized value byte
		Preset			= 1,	///< Activate a Preset
		EffectTrigger	= 2		///< Play/Stop/Toggle an EffectLayer
	};

	/**
	 * Declarative, GUI-authorable MIDI mapping: a message filter (mirroring nap::MidiInputComponent's)
	 * plus a target to drive when a message passes the filter. Unlike the hand-authored
	 * Entity+MidiInputComponent+MidiParameterComponent/MidiTriggerComponent pattern, a MidiMapping is
	 * pure data with no accompanying Entity/Component of its own - it is matched procedurally by
	 * lxcontrolService against every incoming MidiEvent it receives from one shared, wildcard-filtered
	 * MidiInputComponent. This makes mappings first-class objects a GUI can list/create/delete at runtime.
	 */
	class NAPAPI MidiMapping : public Resource
	{
		RTTI_ENABLE(Resource)
	public:
		virtual bool init(utility::ErrorState& errorState) override;

		/**
		 * @param event the incoming midi message
		 * @return whether this mapping's filter matches the given event
		 */
		bool matches(const MidiEvent& event) const;

		// Filter properties, mirroring nap::MidiInputComponent
		std::vector<std::string>	mPorts;					///< Property: 'Ports' empty means all ports
		std::vector<MidiValue>		mChannels;				///< Property: 'Channels' empty means all channels
		std::vector<MidiValue>		mNumbers;				///< Property: 'Numbers' empty means all numbers
		bool						mNoteOn = false;		///< Property: 'NoteOn'
		bool						mNoteOff = false;		///< Property: 'NoteOff'
		bool						mAftertouch = false;	///< Property: 'Aftertouch'
		bool						mControlChange = false;	///< Property: 'ControlChange'
		bool						mProgramChange = false;	///< Property: 'ProgramChange'
		bool						mChannelPressure = false;	///< Property: 'ChannelPressure'
		bool						mPitchBend = false;		///< Property: 'PitchBend'

		// Target
		EMidiMappingTargetKind		mTargetKind = EMidiMappingTargetKind::Parameter;	///< Property: 'TargetKind'

		ResourcePtr<ParameterFloat>	mParameter;				///< Property: 'Parameter' used when TargetKind == Parameter
		float						mInputMinimum = 0.0f;	///< Property: 'InputMinimum' used when TargetKind == Parameter
		float						mInputMaximum = 127.0f;	///< Property: 'InputMaximum' used when TargetKind == Parameter

		ResourcePtr<Preset>			mPreset;				///< Property: 'Preset' used when TargetKind == Preset

		ResourcePtr<EffectLayer>	mEffectLayer;			///< Property: 'EffectLayer' used when TargetKind == EffectTrigger
		EMidiTriggerAction			mAction = EMidiTriggerAction::Toggle;	///< Property: 'Action' used when TargetKind == EffectTrigger
	};
}
