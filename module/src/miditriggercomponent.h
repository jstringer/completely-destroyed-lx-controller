#pragma once

// External Includes
#include <component.h>
#include <midievent.h>
#include <nap/signalslot.h>

// Local Includes
#include "effectlayer.h"

namespace nap
{
	class MidiTriggerComponentInstance;

	/**
	 * Discrete action to perform on the target EffectLayer's SequencePlayer.
	 */
	enum class EMidiTriggerAction : int
	{
		Play	= 0,
		Stop	= 1,
		Toggle	= 2
	};

	/**
	 * Generic discrete MIDI trigger: on any message that passes the sibling
	 * nap::MidiInputComponent's filter, performs 'Action' (Play/Stop/Toggle) on
	 * 'TargetLayer's SequencePlayer. Covers "trigger effect" / "turn effect on-off" mappings
	 * without one bespoke component per effect.
	 */
	class NAPAPI MidiTriggerComponent : public Component
	{
		RTTI_ENABLE(Component)
		DECLARE_COMPONENT(MidiTriggerComponent, MidiTriggerComponentInstance)
	public:
		MidiTriggerComponent() : Component() { }

		virtual void getDependentComponents(std::vector<rtti::TypeInfo>& components) const override;

		ResourcePtr<EffectLayer>	mTargetLayer;						///< Property: 'TargetLayer'
		EMidiTriggerAction			mAction = EMidiTriggerAction::Toggle;	///< Property: 'Action'
	};


	class NAPAPI MidiTriggerComponentInstance : public ComponentInstance
	{
		RTTI_ENABLE(ComponentInstance)
	public:
		MidiTriggerComponentInstance(EntityInstance& entity, Component& resource) :
			ComponentInstance(entity, resource) { }

		virtual bool init(utility::ErrorState& errorState) override;

	private:
		void onEventReceived(const MidiEvent& event);
		Slot<const MidiEvent&> mEventReceivedSlot = { this, &MidiTriggerComponentInstance::onEventReceived };

		EffectLayer*		mTargetLayer = nullptr;
		EMidiTriggerAction	mAction = EMidiTriggerAction::Toggle;
	};
}
