#include "miditriggercomponent.h"

#include <entity.h>
#include <midiinputcomponent.h>

RTTI_BEGIN_ENUM(nap::EMidiTriggerAction)
	RTTI_ENUM_VALUE(nap::EMidiTriggerAction::Play,		"Play"),
	RTTI_ENUM_VALUE(nap::EMidiTriggerAction::Stop,		"Stop"),
	RTTI_ENUM_VALUE(nap::EMidiTriggerAction::Toggle,	"Toggle")
RTTI_END_ENUM

RTTI_BEGIN_CLASS(nap::MidiTriggerComponent)
	RTTI_PROPERTY("TargetLayer",	&nap::MidiTriggerComponent::mTargetLayer,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Action",		&nap::MidiTriggerComponent::mAction,		nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::MidiTriggerComponentInstance)
	RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)
RTTI_END_CLASS

namespace nap
{
	void MidiTriggerComponent::getDependentComponents(std::vector<rtti::TypeInfo>& components) const
	{
		components.emplace_back(RTTI_OF(MidiInputComponent));
	}


	bool MidiTriggerComponentInstance::init(utility::ErrorState& errorState)
	{
		auto* resource = getComponent<MidiTriggerComponent>();
		mTargetLayer = resource->mTargetLayer.get();
		mAction = resource->mAction;
		if (!errorState.check(mTargetLayer != nullptr, "%s: missing TargetLayer", mID.c_str()))
			return false;

		MidiInputComponentInstance* midi_input = getEntityInstance()->findComponent<MidiInputComponentInstance>();
		if (!errorState.check(midi_input != nullptr, "%s: missing sibling MidiInputComponent", mID.c_str()))
			return false;

		midi_input->messageReceived.connect(mEventReceivedSlot);
		return true;
	}


	void MidiTriggerComponentInstance::onEventReceived(const MidiEvent& event)
	{
		SequencePlayer& player = *mTargetLayer->mPlayer;
		switch (mAction)
		{
		case EMidiTriggerAction::Play:
			player.setIsPlaying(true);
			break;
		case EMidiTriggerAction::Stop:
			player.setIsPlaying(false);
			break;
		case EMidiTriggerAction::Toggle:
			player.setIsPlaying(!player.getIsPlaying());
			break;
		}
	}
}
