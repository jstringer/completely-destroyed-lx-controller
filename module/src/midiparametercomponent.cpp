#include "midiparametercomponent.h"

#include <entity.h>
#include <midiinputcomponent.h>
#include <parameternumeric.h>
#include <mathutils.h>

RTTI_BEGIN_CLASS(nap::MidiParameterComponent)
	RTTI_PROPERTY("Parameter",		&nap::MidiParameterComponent::mParameter,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("InputMinimum",	&nap::MidiParameterComponent::mInputMinimum,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("InputMaximum",	&nap::MidiParameterComponent::mInputMaximum,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::MidiParameterComponentInstance)
	RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)
RTTI_END_CLASS

namespace nap
{
	/**
	 * Normalizes a MIDI event's value (CC value, note velocity, channel pressure, or pitch bend)
	 * from ['inputMinimum', 'inputMaximum'] into [0, 1]. Returns false if the event type isn't
	 * one this mapping understands.
	 */
	static bool getNormalizedMidiValue(const MidiEvent& event, float inputMinimum, float inputMaximum, float& outNormalized)
	{
		float raw = 0.0f;
		switch (event.getType())
		{
		case MidiEvent::Type::controlChange:
			raw = static_cast<float>(event.getCCValue());
			break;
		case MidiEvent::Type::noteOn:
		case MidiEvent::Type::noteOff:
			raw = static_cast<float>(event.getVelocity());
			break;
		case MidiEvent::Type::channelPressure:
			raw = static_cast<float>(event.getValue());
			break;
		case MidiEvent::Type::pitchBend:
			outNormalized = math::clamp((event.getPitchBendValue() + 1.0f) * 0.5f, 0.0f, 1.0f);
			return true;
		default:
			return false;
		}

		float range = inputMaximum - inputMinimum;
		outNormalized = range > 0.0f ? math::clamp((raw - inputMinimum) / range, 0.0f, 1.0f) : 0.0f;
		return true;
	}


	/**
	 * If 'parameter' is of type ParamType, lerps 'normalized' into its [Minimum, Maximum] and calls setValue().
	 * @return whether 'parameter' matched ParamType (and was therefore handled)
	 */
	template<typename ParamType, typename ValueType>
	static bool applyNormalizedValue(Parameter& parameter, float normalized)
	{
		auto* typed = rtti_cast<ParamType>(&parameter);
		if (typed == nullptr)
			return false;

		ValueType value = static_cast<ValueType>(math::lerp<float>(static_cast<float>(typed->mMinimum), static_cast<float>(typed->mMaximum), normalized));
		typed->setValue(value);
		return true;
	}


	void MidiParameterComponent::getDependentComponents(std::vector<rtti::TypeInfo>& components) const
	{
		components.emplace_back(RTTI_OF(MidiInputComponent));
	}


	bool MidiParameterComponentInstance::init(utility::ErrorState& errorState)
	{
		auto* resource = getComponent<MidiParameterComponent>();
		mParameter = resource->mParameter.get();
		mInputMinimum = resource->mInputMinimum;
		mInputMaximum = resource->mInputMaximum;
		if (!errorState.check(mParameter != nullptr, "%s: missing target Parameter", mID.c_str()))
			return false;

		MidiInputComponentInstance* midi_input = getEntityInstance()->findComponent<MidiInputComponentInstance>();
		if (!errorState.check(midi_input != nullptr, "%s: missing sibling MidiInputComponent", mID.c_str()))
			return false;

		midi_input->messageReceived.connect(mEventReceivedSlot);
		return true;
	}


	void MidiParameterComponentInstance::onEventReceived(const MidiEvent& event)
	{
		float normalized = 0.0f;
		if (!getNormalizedMidiValue(event, mInputMinimum, mInputMaximum, normalized))
			return;

		// Try each supported numeric parameter type until one matches the runtime type of mParameter
		if (applyNormalizedValue<ParameterFloat, float>(*mParameter, normalized))		return;
		if (applyNormalizedValue<ParameterInt, int>(*mParameter, normalized))			return;
		if (applyNormalizedValue<ParameterDouble, double>(*mParameter, normalized))	return;
		if (applyNormalizedValue<ParameterUInt, uint>(*mParameter, normalized))		return;
		if (applyNormalizedValue<ParameterByte, uint8_t>(*mParameter, normalized))		return;
		applyNormalizedValue<ParameterLong, int64_t>(*mParameter, normalized);
	}
}
