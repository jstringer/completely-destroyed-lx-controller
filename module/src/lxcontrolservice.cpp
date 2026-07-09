// Local Includes
#include "lxcontrolservice.h"

// External Includes
#include <nap/core.h>
#include <nap/resourcemanager.h>
#include <nap/logger.h>
#include <parameterservice.h>
#include <rtti/jsonwriter.h>
#include <rtti/writer.h>
#include <sequenceplayerclock.h>
#include <sequenceplayercurveoutput.h>
#include <utility/fileutils.h>
#include <mathutils.h>
#include <fstream>

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::lxcontrolService)
	RTTI_CONSTRUCTOR(nap::ServiceConfiguration*)
RTTI_END_CLASS

namespace nap
{
	static constexpr const char* sUserContentFile = "user_content.json";


	bool lxcontrolService::init(nap::utility::ErrorState& errorState)
	{
		mResourceManager = getCore().getResourceManager();
		return true;
	}


	void lxcontrolService::update(double deltaTime)
	{
		if (mMidiPort == nullptr || mMidiHotplugMonitor == nullptr)
			return;

		std::vector<std::string> new_ports;
		if (!mMidiHotplugMonitor->update(deltaTime, new_ports))
			return;

		mMidiPort->stop();
		utility::ErrorState error;
		if (mMidiPort->start(error))
			mMidiLog.push_front("MIDI devices changed - reconnected: " + mMidiPort->getPortNames());
		else
			mMidiLog.push_front("MIDI devices changed - reconnect failed: " + error.toString());
		while (mMidiLog.size() > sMaxMidiLogSize)
			mMidiLog.pop_back();
	}


	void lxcontrolService::getDependentServices(std::vector<rtti::TypeInfo>& dependencies)
	{
	}


	void lxcontrolService::shutdown()
	{
	}


	std::string lxcontrolService::makeUniqueID(const std::string& base) const
	{
		if (mResourceManager->findObject(base) == nullptr)
			return base;

		int suffix = 2;
		while (true)
		{
			std::string candidate = base + "_" + std::to_string(suffix);
			if (mResourceManager->findObject(candidate) == nullptr)
				return candidate;
			suffix++;
		}
	}


	FixtureChannelBinding* lxcontrolService::findBinding(Fixture& fixture, const std::string& channelName) const
	{
		for (auto& binding : fixture.mChannelBindings)
		{
			if (binding->mChannelName == channelName)
				return binding.get();
		}
		return nullptr;
	}


	static std::string fixturePresetFile(const std::string& baseId, const std::string& fixtureId)
	{
		return baseId + "_" + fixtureId + ".json";
	}


	bool lxcontrolService::setup(const std::vector<ResourcePtr<ParameterGroup>>& fixtureParams, ResourcePtr<RenderWindow> renderWindow,
		const std::vector<ResourcePtr<Fixture>>& fixtures, MidiInputComponentInstance& midiSource,
		ResourcePtr<MidiInputPort> midiPort, utility::ErrorState& errorState)
	{
		mFixtureParams = fixtureParams;
		mRenderWindow = renderWindow;
		for (auto& fixture : fixtures)
			mFixtures.emplace_back(fixture.get());

		mMidiPort = midiPort;
		mMidiHotplugMonitor = std::make_unique<MidiHotplugMonitor>();

		midiSource.messageReceived.connect(mMidiSlot);

		if (utility::fileExists(sUserContentFile))
		{
			if (!mResourceManager->loadFile(sUserContentFile, errorState))
				return false;
			rebuildFromLoadedContent();
		}

		return true;
	}


	void lxcontrolService::rebuildFromLoadedContent()
	{
		for (auto& preset : mResourceManager->getObjects<Preset>())
			mPresets.emplace_back(preset);

		for (auto& mapping : mResourceManager->getObjects<MidiMapping>())
			mMidiMappings.emplace_back(mapping);

		auto all_editors = mResourceManager->getObjects<SequenceEditor>();
		auto all_editor_guis = mResourceManager->getObjects<SequenceEditorGUI>();
		auto all_links = mResourceManager->getObjects<EffectLayerOverride>();

		for (auto& layer : mResourceManager->getObjects<EffectLayer>())
		{
			EffectLayerEntry entry;
			entry.mLayer = layer;
			entry.mPlayer = rtti::ObjectPtr<SequencePlayer>(layer->mPlayer.get());

			for (auto& editor : all_editors)
			{
				if (editor->mSequencePlayer.get() == entry.mPlayer.get())
				{
					entry.mEditor = editor;
					break;
				}
			}
			if (entry.mEditor != nullptr)
			{
				for (auto& gui : all_editor_guis)
				{
					if (gui->mSequenceEditor.get() == entry.mEditor.get())
					{
						entry.mEditorGUI = gui;
						break;
					}
				}
			}

			for (auto& output : entry.mPlayer->mOutputs)
			{
				auto* curve_output = rtti_cast<SequencePlayerCurveOutput>(output.get());
				if (curve_output == nullptr)
					continue;

				auto* float_param = rtti_cast<ParameterFloat>(curve_output->mParameter.get());
				if (float_param != nullptr)
					entry.mOverrideParams.emplace_back(float_param);
			}

			for (auto& link : all_links)
			{
				if (link->mLayer.get() == layer.get())
				{
					entry.mOverrideLinks.emplace_back(link);
					if (link->mTargetBinding != nullptr)
						link->mTargetBinding->addOverride(link);
				}
			}

			mEffectEntries.emplace_back(entry);
			mEffectLayers.emplace_back(layer);
		}
	}


	void lxcontrolService::save()
	{
		rtti::ObjectList root_objects;
		for (auto& preset : mPresets)
			root_objects.emplace_back(preset.get());

		for (auto& entry : mEffectEntries)
		{
			if (entry.mRemoved)
				continue;

			root_objects.emplace_back(entry.mLayer.get());
			root_objects.emplace_back(entry.mPlayer.get());
			if (entry.mEditor != nullptr)
				root_objects.emplace_back(entry.mEditor.get());
			if (entry.mEditorGUI != nullptr)
				root_objects.emplace_back(entry.mEditorGUI.get());
			for (auto& param : entry.mOverrideParams)
				root_objects.emplace_back(param.get());
			for (auto& link : entry.mOverrideLinks)
				root_objects.emplace_back(link.get());
		}

		for (auto& mapping : mMidiMappings)
			root_objects.emplace_back(mapping.get());

		rtti::JSONWriter writer;
		utility::ErrorState error;
		if (!rtti::serializeObjects(root_objects, writer, error))
		{
			Logger::error("lxcontrolService: failed to serialize user content: %s", error.toString().c_str());
			return;
		}

		std::ofstream out(sUserContentFile);
		out << writer.GetJSON();
	}


	Preset* lxcontrolService::createPreset(const std::string& name, utility::ErrorState& errorState)
	{
		auto preset = mResourceManager->createObject<Preset>();
		preset->mID = makeUniqueID("Preset_" + name);
		preset->mName = name;
		preset->mPresetFile = preset->mID;	// base id; per-fixture files are derived from this

		auto* param_service = getCore().getService<ParameterService>();
		for (size_t i = 0; i < mFixtures.size(); ++i)
		{
			if (!param_service->savePreset(*mFixtureParams[i], fixturePresetFile(preset->mPresetFile, mFixtures[i]->mID), errorState))
				return nullptr;
		}

		if (!preset->init(errorState))
			return nullptr;

		mPresets.emplace_back(preset);
		save();
		return preset.get();
	}


	void lxcontrolService::removePreset(Preset* preset)
	{
		mPresets.erase(std::remove_if(mPresets.begin(), mPresets.end(), [preset](const rtti::ObjectPtr<Preset>& p)
		{
			return p.get() == preset;
		}), mPresets.end());
		save();
	}


	void lxcontrolService::applyPreset(Preset* preset, utility::ErrorState& errorState)
	{
		if (preset == nullptr)
			return;

		auto* param_service = getCore().getService<ParameterService>();
		for (size_t i = 0; i < mFixtures.size(); ++i)
			param_service->loadPreset(*mFixtureParams[i], fixturePresetFile(preset->mPresetFile, mFixtures[i]->mID), errorState);

		for (auto& entry : mEffectEntries)
		{
			if (entry.mRemoved)
				continue;
			entry.mPlayer->setIsPlaying(preset->playsLayer(*entry.mLayer));
		}
	}


	void lxcontrolService::resnapshotPreset(Preset* preset, utility::ErrorState& errorState)
	{
		if (preset == nullptr)
			return;

		auto* param_service = getCore().getService<ParameterService>();
		for (size_t i = 0; i < mFixtures.size(); ++i)
			param_service->savePreset(*mFixtureParams[i], fixturePresetFile(preset->mPresetFile, mFixtures[i]->mID), errorState);
	}


	EffectLayer* lxcontrolService::createEffectLayer(const std::string& name, int priority, const std::vector<EffectTarget>& targets, utility::ErrorState& errorState)
	{
		std::string base_id = makeUniqueID("Effect_" + name);

		auto clock = mResourceManager->createObject<SequencePlayerStandardClock>();
		clock->mID = makeUniqueID(base_id + "_Clock");
		if (!clock->init(errorState))
			return nullptr;

		auto player = mResourceManager->createObject<SequencePlayer>();
		player->mID = makeUniqueID(base_id + "_Player");
		player->mSequenceFileName = base_id + ".json";
		player->mCreateEmptySequenceOnLoadFail = true;
		player->mClock = clock;

		EffectLayerEntry entry;

		for (const auto& target : targets)
		{
			FixtureChannelBinding* binding = findBinding(*target.mFixture, target.mChannelName);
			if (!errorState.check(binding != nullptr, "%s: fixture %s has no channel named %s", base_id.c_str(), target.mFixture->mID.c_str(), target.mChannelName.c_str()))
				return nullptr;

			std::string target_id = base_id + "_" + target.mFixture->mID + "_" + target.mChannelName;

			auto override_param = mResourceManager->createObject<ParameterFloat>();
			override_param->mID = makeUniqueID(target_id + "_Override");
			override_param->mValue = 0.0f;
			override_param->mMinimum = 0.0f;
			override_param->mMaximum = 1.0f;
			if (!override_param->init(errorState))
				return nullptr;

			auto curve_output = mResourceManager->createObject<SequencePlayerCurveOutput>();
			curve_output->mID = makeUniqueID(target_id + "_Output");
			curve_output->mParameter = rtti::ObjectPtr<Parameter>(override_param.get());
			curve_output->mUseMainThread = true;
			if (!curve_output->init(errorState))
				return nullptr;

			player->mOutputs.emplace_back(rtti::ObjectPtr<SequencePlayerOutput>(curve_output.get()));
			entry.mOverrideParams.emplace_back(override_param);

			auto override_link = mResourceManager->createObject<EffectLayerOverride>();
			override_link->mID = makeUniqueID(target_id + "_Link");
			override_link->mOverrideParameter = override_param;
			override_link->mTargetBinding = binding;
			entry.mOverrideLinks.emplace_back(override_link);
		}

		if (!player->init(errorState))
			return nullptr;
		if (!player->start(errorState))
			return nullptr;

		auto layer = mResourceManager->createObject<EffectLayer>();
		layer->mID = makeUniqueID(base_id + "_Layer");
		layer->mName = name;
		layer->mPlayer = player;
		layer->mPriority = priority;
		if (!layer->init(errorState))
			return nullptr;

		// Finish initializing the override links now that the layer exists, and attach them
		for (auto& link : entry.mOverrideLinks)
		{
			link->mLayer = layer;
			if (!link->init(errorState))
				return nullptr;
			link->mTargetBinding->addOverride(link);
		}

		auto editor = mResourceManager->createObject<SequenceEditor>();
		editor->mID = makeUniqueID(base_id + "_Editor");
		editor->mSequencePlayer = player;
		if (!editor->init(errorState))
			return nullptr;

		auto editor_gui = mResourceManager->createObject<SequenceEditorGUI>();
		editor_gui->mID = makeUniqueID(base_id + "_EditorGUI");
		editor_gui->mSequenceEditor = editor;
		editor_gui->mRenderWindow = mRenderWindow;
		editor_gui->mDrawFullWindow = false;
		editor_gui->mHideMarkerLabels = false;
		if (!editor_gui->init(errorState))
			return nullptr;

		entry.mLayer = layer;
		entry.mPlayer = player;
		entry.mEditor = editor;
		entry.mEditorGUI = editor_gui;

		mEffectEntries.emplace_back(entry);
		mEffectLayers.emplace_back(layer);
		save();
		return layer.get();
	}


	void lxcontrolService::removeEffectLayer(EffectLayer* layer)
	{
		for (auto& entry : mEffectEntries)
		{
			if (entry.mLayer.get() != layer)
				continue;

			entry.mPlayer->setIsPlaying(false);
			entry.mRemoved = true;
			break;
		}

		mEffectLayers.erase(std::remove_if(mEffectLayers.begin(), mEffectLayers.end(), [layer](const rtti::ObjectPtr<EffectLayer>& l)
		{
			return l.get() == layer;
		}), mEffectLayers.end());
		save();
	}


	SequenceEditorGUI* lxcontrolService::getEffectEditorGUI(EffectLayer* layer) const
	{
		for (auto& entry : mEffectEntries)
		{
			if (entry.mLayer.get() == layer)
				return entry.mEditorGUI.get();
		}
		return nullptr;
	}


	MidiMapping* lxcontrolService::createMidiMapping(const MidiEvent& learnedEvent, EMidiMappingTargetKind kind,
		ParameterFloat* parameter, float inputMinimum, float inputMaximum,
		Preset* preset, EffectLayer* effectLayer, EMidiTriggerAction action,
		utility::ErrorState& errorState)
	{
		auto mapping = mResourceManager->createObject<MidiMapping>();
		mapping->mID = makeUniqueID("MidiMapping_" + std::to_string(mMidiMappings.size() + 1));

		mapping->mNumbers = { learnedEvent.getNumber() };
		switch (learnedEvent.getType())
		{
		case MidiEvent::Type::noteOn:			mapping->mNoteOn = true;			break;
		case MidiEvent::Type::noteOff:			mapping->mNoteOff = true;			break;
		case MidiEvent::Type::afterTouch:		mapping->mAftertouch = true;		break;
		case MidiEvent::Type::controlChange:	mapping->mControlChange = true;		break;
		case MidiEvent::Type::programChange:	mapping->mProgramChange = true;		break;
		case MidiEvent::Type::channelPressure:	mapping->mChannelPressure = true;	break;
		case MidiEvent::Type::pitchBend:		mapping->mPitchBend = true;			break;
		}

		mapping->mTargetKind = kind;
		mapping->mParameter = parameter;
		mapping->mInputMinimum = inputMinimum;
		mapping->mInputMaximum = inputMaximum;
		mapping->mPreset = preset;
		mapping->mEffectLayer = effectLayer;
		mapping->mAction = action;

		if (!mapping->init(errorState))
			return nullptr;

		mMidiMappings.emplace_back(mapping);
		save();
		return mapping.get();
	}


	void lxcontrolService::removeMidiMapping(MidiMapping* mapping)
	{
		mMidiMappings.erase(std::remove_if(mMidiMappings.begin(), mMidiMappings.end(), [mapping](const rtti::ObjectPtr<MidiMapping>& m)
		{
			return m.get() == mapping;
		}), mMidiMappings.end());
		save();
	}


	void lxcontrolService::onMidiEvent(const MidiEvent& event)
	{
		mMidiLog.push_front(event.toString());
		while (mMidiLog.size() > sMaxMidiLogSize)
			mMidiLog.pop_back();

		mLastEventType = event.getType();
		mLastEventNumber = event.getNumber();
		mLastEventValue = event.getValue();
		mLastEventChannel = event.getChannel();
		mLastEventPort = event.getPort();
		mHasLastEvent = true;
		mMidiEventCounter++;

		utility::ErrorState error;
		for (auto& mapping : mMidiMappings)
		{
			if (!mapping->matches(event))
				continue;

			switch (mapping->mTargetKind)
			{
			case EMidiMappingTargetKind::Parameter:
			{
				if (mapping->mParameter == nullptr)
					break;

				float raw = static_cast<float>(event.getValue());
				float range = mapping->mInputMaximum - mapping->mInputMinimum;
				float normalized = range > 0.0f ? math::clamp((raw - mapping->mInputMinimum) / range, 0.0f, 1.0f) : 0.0f;
				auto* param = mapping->mParameter.get();
				param->setValue(math::lerp<float>(param->mMinimum, param->mMaximum, normalized));
				break;
			}
			case EMidiMappingTargetKind::Preset:
				if (mapping->mPreset != nullptr)
					applyPreset(mapping->mPreset.get(), error);
				break;
			case EMidiMappingTargetKind::EffectTrigger:
				if (mapping->mEffectLayer != nullptr && mapping->mEffectLayer->mPlayer != nullptr)
				{
					SequencePlayer& player = *mapping->mEffectLayer->mPlayer;
					switch (mapping->mAction)
					{
					case EMidiTriggerAction::Play:		player.setIsPlaying(true);					break;
					case EMidiTriggerAction::Stop:		player.setIsPlaying(false);					break;
					case EMidiTriggerAction::Toggle:	player.setIsPlaying(!player.getIsPlaying());	break;
					}
				}
				break;
			}
		}
	}
}
