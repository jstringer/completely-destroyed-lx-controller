#pragma once

// External Includes
#include <component.h>
#include <midievent.h>
#include <parameter.h>
#include <nap/signalslot.h>

namespace nap
{
	class MidiParameterComponentInstance;

	/**
	 * Generic continuous MIDI-to-parameter mapping: normalizes an incoming MIDI value (CC value,
	 * note velocity, channel pressure, or pitch bend) from ['InputMinimum', 'InputMaximum'] into
	 * [0, 1] and applies it to 'Parameter'. 'Parameter' can be any numeric nap::Parameter
	 * (ParameterFloat, ParameterInt, ...); the concrete type is resolved at runtime, so a single
	 * mapping component covers fixture faders, preset index selection, blend time, etc. Requires
	 * a sibling nap::MidiInputComponent to filter which messages qualify (port/channel/number/type).
	 */
	class NAPAPI MidiParameterComponent : public Component
	{
		RTTI_ENABLE(Component)
		DECLARE_COMPONENT(MidiParameterComponent, MidiParameterComponentInstance)
	public:
		MidiParameterComponent() : Component() { }

		virtual void getDependentComponents(std::vector<rtti::TypeInfo>& components) const override;

		ResourcePtr<Parameter>	mParameter;					///< Property: 'Parameter' target parameter to drive (any numeric Parameter type)
		float					mInputMinimum = 0.0f;		///< Property: 'InputMinimum' raw midi value mapped to the parameter's minimum
		float					mInputMaximum = 127.0f;	///< Property: 'InputMaximum' raw midi value mapped to the parameter's maximum
	};


	class NAPAPI MidiParameterComponentInstance : public ComponentInstance
	{
		RTTI_ENABLE(ComponentInstance)
	public:
		MidiParameterComponentInstance(EntityInstance& entity, Component& resource) :
			ComponentInstance(entity, resource) { }

		virtual bool init(utility::ErrorState& errorState) override;

	private:
		void onEventReceived(const MidiEvent& event);
		Slot<const MidiEvent&> mEventReceivedSlot = { this, &MidiParameterComponentInstance::onEventReceived };

		Parameter*	mParameter = nullptr;
		float		mInputMinimum = 0.0f;
		float		mInputMaximum = 127.0f;
	};
}
