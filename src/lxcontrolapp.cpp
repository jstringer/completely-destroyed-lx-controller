#include "lxcontrolapp.h"

// External Includes
#include <utility/fileutils.h>
#include <nap/logger.h>
#include <inputrouter.h>
#include <rendergnomoncomponent.h>
#include <perspcameracomponent.h>
#include <imgui/imgui.h>
#include <mathutils.h>
#include <cstring>
#include <algorithm>

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::lxcontrolApp)
	RTTI_CONSTRUCTOR(nap::Core&)
RTTI_END_CLASS

namespace nap
{
	/**
	 * Initialize all the resources and instances used for drawing
	 * slowly migrating all functionality to NAP
	 */
	bool lxcontrolApp::init(utility::ErrorState& error)
	{
		// Retrieve services
		mRenderService		= getCore().getService<nap::RenderService>();
		mSceneService		= getCore().getService<nap::SceneService>();
		mInputService		= getCore().getService<nap::InputService>();
		mGuiService			= getCore().getService<nap::IMGuiService>();
		mLxControlService	= getCore().getService<nap::lxcontrolService>();

		// Fetch the resource manager
		mResourceManager = getCore().getResourceManager();

		// Get the render window
		mRenderWindow = mResourceManager->findObject<nap::RenderWindow>("Window");
		if (!error.check(mRenderWindow != nullptr, "unable to find render window with name: %s", "Window"))
			return false;

		// Get the scene that contains our entities and components
		mScene = mResourceManager->findObject<Scene>("Scene");
		if (!error.check(mScene != nullptr, "unable to find scene with name: %s", "Scene"))
			return false;

		// Get the camera entity
		mCameraEntity = mScene->findEntity("CameraEntity");
		if (!error.check(mCameraEntity != nullptr, "unable to find entity with name: %s", "CameraEntity"))
			return false;

		// Get the Gnomon entity
		mGnomonEntity = mScene->findEntity("GnomonEntity");
		if (!error.check(mGnomonEntity != nullptr, "unable to find entity with name: %s", "GnomonEntity"))
			return false;

		// Gather the fixture rig and hand it, plus the wildcard MIDI listener, to lxcontrolService
		std::vector<ResourcePtr<Fixture>> fixtures;
		std::vector<ResourcePtr<ParameterGroup>> fixture_params;
		for (const char* fixture_id : { "Fixture_Strobe1", "Fixture_Strobe2", "Fixture_Strobe3" })
		{
			auto fixture = mResourceManager->findObject<Fixture>(fixture_id);
			if (!error.check(fixture != nullptr, "unable to find object with name: %s", fixture_id))
				return false;
			fixtures.emplace_back(fixture);
		}
		for (const char* group_id : { "Strobe1_Params", "Strobe2_Params", "Strobe3_Params" })
		{
			auto group = mResourceManager->findObject<ParameterGroup>(group_id);
			if (!error.check(group != nullptr, "unable to find object with name: %s", group_id))
				return false;
			fixture_params.emplace_back(group);
		}

		// Keep the same three groups around for the Fixtures tab's own custom draw routine
		mFixtureParams1 = fixture_params[0];
		mFixtureParams2 = fixture_params[1];
		mFixtureParams3 = fixture_params[2];

		auto midi_monitor_entity = mScene->findEntity("MidiMonitorEntity");
		if (!error.check(midi_monitor_entity != nullptr, "unable to find entity with name: %s", "MidiMonitorEntity"))
			return false;

		auto* midi_input = midi_monitor_entity->findComponent<MidiInputComponentInstance>();
		if (!error.check(midi_input != nullptr, "MidiMonitorEntity is missing its MidiInputComponent"))
			return false;

		mMidiPort = mResourceManager->findObject<MidiInputPort>("MidiPort");
		if (!error.check(mMidiPort != nullptr, "unable to find object with name: %s", "MidiPort"))
			return false;

		if (!mLxControlService->setup(fixture_params, mRenderWindow, fixtures, *midi_input, mMidiPort, error))
			return false;

		// All done!
		return true;
	}


	// Update app
	void lxcontrolApp::update(double deltaTime)
	{
		// Use a default input router to forward input events (recursively) to all input components in the default scene
		nap::DefaultInputRouter input_router(true);
		mInputService->processWindowEvents(*mRenderWindow, input_router, { &mScene->getRootEntity() });

		// Bind subsequent ImGui calls to our single window, then queue our GUIs.
		// These must be shown here (update), not in render() -- render() only draws what was queued here.
		mGuiService->selectWindow(mRenderWindow);
		drawMainUI();

		// Keep any effect timelines the user opened visible, regardless of which tab is active
		for (auto* layer : mOpenTimelines)
		{
			auto* gui = mLxControlService->getEffectEditorGUI(layer);
			if (gui != nullptr)
				gui->show(true);
		}
	}


	// Draws a fixture's parameter group, rendering contiguous R/G/B channel triplets
	// (detected by display-name suffix, e.g. "Unit1 R"/"Unit1 G"/"Unit1 B") as a single
	// ImGui::ColorEdit3 swatch instead of three separate sliders. Every other parameter
	// keeps rendering as a plain slider, in its original declared order.
	void lxcontrolApp::drawFixtureParamGroup(ParameterGroup& group)
	{
		auto& members = group.mMembers;
		int swatches_on_line = 0;
		for (size_t i = 0; i < members.size(); )
		{
			auto* r = rtti_cast<ParameterFloat>(members[i].get());
			if (r == nullptr)
			{
				i++;
				continue;
			}
			std::string name = r->getDisplayName();

			ParameterFloat *g = nullptr, *b = nullptr;
			bool is_triplet = false;
			if (name.size() > 2 && name.compare(name.size() - 2, 2, " R") == 0 && i + 2 < members.size())
			{
				std::string prefix = name.substr(0, name.size() - 2);
				g = rtti_cast<ParameterFloat>(members[i + 1].get());
				b = rtti_cast<ParameterFloat>(members[i + 2].get());
				is_triplet = g != nullptr && b != nullptr &&
					g->getDisplayName() == prefix + " G" &&
					b->getDisplayName() == prefix + " B";
			}

			ImGui::PushID(static_cast<int>(i));
			if (is_triplet)
			{
				float col[3] = { r->mValue, g->mValue, b->mValue };
				std::string label = name.substr(0, name.size() - 2);
				if (ImGui::ColorEdit3(label.c_str(), col, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs))
				{
					r->setValue(col[0]);
					g->setValue(col[1]);
					b->setValue(col[2]);
				}
				if (swatches_on_line < 2)
				{
					ImGui::SameLine();
					swatches_on_line++;
				}
				else
				{
					swatches_on_line = 0;
				}
				i += 3;
			}
			else
			{
				float v = r->mValue;
				if (ImGui::SliderFloat(name.c_str(), &v, r->mMinimum, r->mMaximum))
					r->setValue(v);
				swatches_on_line = 0;
				i += 1;
			}
			ImGui::PopID();
		}
	}


	void lxcontrolApp::drawMainUI()
	{
		ImGui::Begin("lxcontrol");
		if (ImGui::BeginTabBar("MainTabs"))
		{
			if (ImGui::BeginTabItem("Fixtures"))
			{
				const char* fixture_names[3] = { "Strobe 1", "Strobe 2", "Strobe 3" };
				ParameterGroup* fixture_groups[3] = { mFixtureParams1.get(), mFixtureParams2.get(), mFixtureParams3.get() };
				for (int i = 0; i < 3; i++)
				{
					if (i > 0)
						ImGui::SameLine();
					ImGui::BeginChild(fixture_names[i], ImVec2(300, 0), true);
					ImGui::Text("%s", fixture_names[i]);
					ImGui::Separator();
					drawFixtureParamGroup(*fixture_groups[i]);
					ImGui::EndChild();
				}
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Presets"))
			{
				drawPresetsTab();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Effects"))
			{
				drawEffectsTab();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("MIDI"))
			{
				drawMidiTab();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();
	}


	int lxcontrolApp::drawEffectLayerCombo(const char* label, const std::vector<rtti::ObjectPtr<EffectLayer>>& layers, EffectLayer* current)
	{
		std::vector<const char*> c_labels;
		c_labels.emplace_back("None");
		for (auto& layer : layers)
			c_labels.emplace_back(layer->mName.c_str());

		int index = 0;
		for (size_t i = 0; i < layers.size(); ++i)
		{
			if (layers[i].get() == current)
			{
				index = static_cast<int>(i) + 1;
				break;
			}
		}

		ImGui::Combo(label, &index, c_labels.data(), static_cast<int>(c_labels.size()));
		return index - 1;
	}


	void lxcontrolApp::drawPresetsTab()
	{
		ImGui::InputText("Name", mNewPresetName, sizeof(mNewPresetName));
		ImGui::SameLine();
		if (ImGui::Button("+ New Preset") && std::strlen(mNewPresetName) > 0)
		{
			utility::ErrorState error;
			if (mLxControlService->createPreset(mNewPresetName, error) == nullptr)
				nap::Logger::error("Failed to create preset: %s", error.toString().c_str());
			mNewPresetName[0] = '\0';
		}

		ImGui::Separator();
		const auto& layers = mLxControlService->getEffectLayers();
		const char* action_labels[] = { "Play", "Stop", "Toggle" };

		for (auto& preset : mLxControlService->getPresets())
		{
			ImGui::PushID(preset.get());
			ImGui::Text("%s", preset->mName.c_str());
			ImGui::SameLine();
			if (ImGui::Button("Activate"))
			{
				utility::ErrorState error;
				mLxControlService->applyPreset(preset.get(), error);
			}
			ImGui::SameLine();
			if (ImGui::Button("Update Snapshot"))
			{
				utility::ErrorState error;
				mLxControlService->resnapshotPreset(preset.get(), error);
			}
			ImGui::SameLine();
			if (ImGui::Button("Delete"))
				mLxControlService->removePreset(preset.get());

			if (ImGui::TreeNode("Effects to play"))
			{
				for (auto& layer : layers)
				{
					bool plays = preset->playsLayer(*layer);
					if (ImGui::Checkbox(layer->mName.c_str(), &plays))
					{
						auto& list = preset->mLayersToPlay;
						if (plays)
						{
							list.emplace_back(layer);
						}
						else
						{
							list.erase(std::remove_if(list.begin(), list.end(), [&layer](const ResourcePtr<EffectLayer>& l)
							{
								return l.get() == layer.get();
							}), list.end());
						}
					}
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("On Enter / On Exit"))
			{
				int enter_action = static_cast<int>(preset->mOnEnterAction);
				int enter_idx = drawEffectLayerCombo("On Enter Effect", layers, preset->mOnEnterEffect.get());
				bool enter_action_changed = ImGui::Combo("On Enter Action", &enter_action, action_labels, 3);
				EffectLayer* enter_effect = (enter_idx >= 0 && enter_idx < static_cast<int>(layers.size())) ? layers[enter_idx].get() : nullptr;
				if (enter_effect != preset->mOnEnterEffect.get() || enter_action_changed)
					mLxControlService->setPresetOnEnter(preset.get(), enter_effect, static_cast<EMidiTriggerAction>(enter_action));

				int exit_action = static_cast<int>(preset->mOnExitAction);
				int exit_idx = drawEffectLayerCombo("On Exit Effect", layers, preset->mOnExitEffect.get());
				bool exit_action_changed = ImGui::Combo("On Exit Action", &exit_action, action_labels, 3);
				EffectLayer* exit_effect = (exit_idx >= 0 && exit_idx < static_cast<int>(layers.size())) ? layers[exit_idx].get() : nullptr;
				if (exit_effect != preset->mOnExitEffect.get() || exit_action_changed)
					mLxControlService->setPresetOnExit(preset.get(), exit_effect, static_cast<EMidiTriggerAction>(exit_action));

				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Note Triggers"))
			{
				for (auto& trigger : preset->mNoteMappings)
				{
					ImGui::PushID(trigger.get());

					int number = static_cast<int>(trigger->mNumber);
					bool number_changed = ImGui::InputInt("Note", &number);

					int effect_idx = drawEffectLayerCombo("Effect", layers, trigger->mEffectLayer.get());
					EffectLayer* effect = (effect_idx >= 0 && effect_idx < static_cast<int>(layers.size())) ? layers[effect_idx].get() : nullptr;

					int action = static_cast<int>(trigger->mAction);
					bool action_changed = ImGui::Combo("Action", &action, action_labels, 3);

					if (number_changed || effect != trigger->mEffectLayer.get() || action_changed)
					{
						mLxControlService->updatePresetNoteMapping(trigger.get(),
							static_cast<MidiValue>(math::clamp(number, 0, 127)), effect, static_cast<EMidiTriggerAction>(action));
					}

					ImGui::SameLine();
					if (ImGui::Button("Remove"))
						mLxControlService->removePresetNoteMapping(preset.get(), trigger.get());

					ImGui::Separator();
					ImGui::PopID();
				}

				auto& form = mNoteMappingForms[preset.get()];
				ImGui::InputInt("New Note", &form.mNumber);
				if (!layers.empty())
				{
					form.mEffectIndex = math::clamp(form.mEffectIndex, 0, static_cast<int>(layers.size()) - 1);
					std::vector<const char*> c_labels;
					for (auto& layer : layers)
						c_labels.emplace_back(layer->mName.c_str());
					ImGui::Combo("New Effect", &form.mEffectIndex, c_labels.data(), static_cast<int>(c_labels.size()));
				}
				ImGui::Combo("New Action", &form.mAction, action_labels, 3);
				if (ImGui::Button("+ Add Note Mapping") && !layers.empty())
				{
					utility::ErrorState error;
					if (mLxControlService->addPresetNoteMapping(preset.get(),
						static_cast<MidiValue>(math::clamp(form.mNumber, 0, 127)),
						layers[form.mEffectIndex].get(),
						static_cast<EMidiTriggerAction>(form.mAction), error) == nullptr)
					{
						nap::Logger::error("Failed to add note mapping: %s", error.toString().c_str());
					}
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
			ImGui::Separator();
		}
	}


	void lxcontrolApp::drawEffectsTab()
	{
		ImGui::InputText("Name", mNewEffectName, sizeof(mNewEffectName));
		ImGui::InputInt("Priority", &mNewEffectPriority);

		const auto& fixtures = mLxControlService->getFixtures();
		size_t total_rows = 0;
		for (auto* fixture : fixtures)
			total_rows += fixture->mChannelBindings.size();
		if (mNewEffectTargets.size() != total_rows)
			mNewEffectTargets.assign(total_rows, false);

		ImGui::Text("Targets:");
		ImGui::BeginChild("EffectTargets", ImVec2(0, 150), true);
		size_t row = 0;
		for (auto* fixture : fixtures)
		{
			ImGui::Text("%s", fixture->mID.c_str());
			for (auto& binding : fixture->mChannelBindings)
			{
				bool checked = mNewEffectTargets[row];
				if (ImGui::Checkbox(binding->mChannelName.c_str(), &checked))
					mNewEffectTargets[row] = checked;
				row++;
			}
		}
		ImGui::EndChild();

		if (ImGui::Button("+ New Effect"))
		{
			std::vector<lxcontrolService::EffectTarget> targets;
			row = 0;
			for (auto* fixture : fixtures)
			{
				for (auto& binding : fixture->mChannelBindings)
				{
					if (mNewEffectTargets[row])
						targets.push_back({ fixture, binding->mChannelName });
					row++;
				}
			}

			if (std::strlen(mNewEffectName) > 0 && !targets.empty())
			{
				utility::ErrorState error;
				if (mLxControlService->createEffectLayer(mNewEffectName, mNewEffectPriority, targets, error) == nullptr)
					nap::Logger::error("Failed to create effect: %s", error.toString().c_str());
				mNewEffectName[0] = '\0';
				std::fill(mNewEffectTargets.begin(), mNewEffectTargets.end(), false);
			}
		}

		ImGui::Separator();
		for (auto& layer : mLxControlService->getEffectLayers())
		{
			ImGui::PushID(layer.get());
			ImGui::Text("%s (priority %d)", layer->mName.c_str(), layer->mPriority);

			ImGui::SameLine();
			bool playing = layer->getIsActive();
			if (ImGui::Checkbox("Playing", &playing))
				layer->mPlayer->setIsPlaying(playing);

			ImGui::SameLine();
			bool timeline_open = mOpenTimelines.find(layer.get()) != mOpenTimelines.end();
			if (ImGui::Checkbox("Timeline", &timeline_open))
			{
				if (timeline_open)
					mOpenTimelines.insert(layer.get());
				else
					mOpenTimelines.erase(layer.get());
			}

			ImGui::SameLine();
			if (ImGui::Button("Delete"))
			{
				mOpenTimelines.erase(layer.get());
				mLxControlService->removeEffectLayer(layer.get());
			}
			ImGui::PopID();
		}
	}


	void lxcontrolApp::drawMidiTab()
	{
		ImGui::Text("Connected MIDI input devices:");
		std::string port_names = mMidiPort->getPortNames();
		if (port_names.empty())
		{
			ImGui::TextDisabled("No MIDI devices found");
		}
		else
		{
			ImGui::BulletText("%s", port_names.c_str());
		}

		ImGui::Separator();
		ImGui::Text("Incoming messages:");
		ImGui::BeginChild("MidiLog", ImVec2(0, 120), true);
		for (const auto& line : mLxControlService->getMidiLog())
			ImGui::TextUnformatted(line.c_str());
		ImGui::EndChild();

		ImGui::Separator();
		ImGui::Text("Learn a new mapping:");

		const char* kind_labels[] = { "Parameter", "Preset" };
		int kind_index = static_cast<int>(mLearnKind);
		if (ImGui::Combo("Target Kind", &kind_index, kind_labels, 2))
			mLearnKind = static_cast<EMidiMappingTargetKind>(kind_index);

		if (mLearnKind == EMidiMappingTargetKind::Parameter)
		{
			// Flatten (fixture, channel) into a combo box
			std::vector<std::string> labels;
			std::vector<ParameterFloat*> params;
			for (auto* fixture : mLxControlService->getFixtures())
			{
				for (auto& binding : fixture->mChannelBindings)
				{
					labels.emplace_back(fixture->mID + " / " + binding->mChannelName);
					params.emplace_back(binding->mBaseParameter.get());
				}
			}
			std::vector<const char*> c_labels;
			for (auto& l : labels)
				c_labels.emplace_back(l.c_str());

			if (!c_labels.empty())
			{
				mLearnParameterIndex = math::clamp(mLearnParameterIndex, 0, static_cast<int>(c_labels.size()) - 1);
				ImGui::Combo("Parameter", &mLearnParameterIndex, c_labels.data(), static_cast<int>(c_labels.size()));
			}
			ImGui::InputFloat("Input Minimum", &mLearnInputMinimum);
			ImGui::InputFloat("Input Maximum", &mLearnInputMaximum);
		}
		else if (mLearnKind == EMidiMappingTargetKind::Preset)
		{
			const auto& presets = mLxControlService->getPresets();
			std::vector<const char*> c_labels;
			for (auto& p : presets)
				c_labels.emplace_back(p->mName.c_str());
			if (!c_labels.empty())
			{
				mLearnPresetIndex = math::clamp(mLearnPresetIndex, 0, static_cast<int>(c_labels.size()) - 1);
				ImGui::Combo("Preset", &mLearnPresetIndex, c_labels.data(), static_cast<int>(c_labels.size()));
			}
		}

		if (!mLearning)
		{
			if (ImGui::Button("Learn..."))
			{
				mLearning = true;
				mLearnStartCounter = mLxControlService->getMidiEventCounter();
			}
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Waiting for a MIDI message...");
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
				mLearning = false;

			if (mLxControlService->getMidiEventCounter() > mLearnStartCounter)
			{
				MidiEvent learned = mLxControlService->getLastMidiEvent();
				utility::ErrorState error;

				ParameterFloat* parameter = nullptr;
				if (mLearnKind == EMidiMappingTargetKind::Parameter)
				{
					int i = 0;
					for (auto* fixture : mLxControlService->getFixtures())
					{
						for (auto& binding : fixture->mChannelBindings)
						{
							if (i == mLearnParameterIndex)
								parameter = binding->mBaseParameter.get();
							i++;
						}
					}
				}

				Preset* preset = nullptr;
				if (mLearnKind == EMidiMappingTargetKind::Preset)
				{
					const auto& presets = mLxControlService->getPresets();
					if (mLearnPresetIndex >= 0 && mLearnPresetIndex < static_cast<int>(presets.size()))
						preset = presets[mLearnPresetIndex].get();
				}

				if (mLxControlService->createMidiMapping(learned, mLearnKind, parameter, mLearnInputMinimum, mLearnInputMaximum,
					preset, error) == nullptr)
				{
					nap::Logger::error("Failed to create MIDI mapping: %s", error.toString().c_str());
				}
				mLearning = false;
			}
		}

		ImGui::Separator();
		ImGui::Text("Existing mappings:");
		for (auto& mapping : mLxControlService->getMidiMappings())
		{
			ImGui::PushID(mapping.get());
			ImGui::BulletText("%s", mapping->mID.c_str());
			ImGui::SameLine();
			if (ImGui::Button("Delete"))
				mLxControlService->removeMidiMapping(mapping.get());
			ImGui::PopID();
		}
	}


	// Render app
	void lxcontrolApp::render()
	{
		// Signal the beginning of a new frame, allowing it to be recorded.
		// The system might wait until all commands that were previously associated with the new frame have been processed on the GPU.
		// Multiple frames are in flight at the same time, but if the graphics load is heavy the system might wait here to ensure resources are available.
		mRenderService->beginFrame();

		// Begin recording the render commands for the main render window
		if (mRenderService->beginRecording(*mRenderWindow))
		{
			// Begin render pass
			mRenderWindow->beginRendering();

			// Get Perspective camera to render with
			auto& perp_cam = mCameraEntity->getComponent<PerspCameraComponentInstance>();

			// Add Gnomon
			std::vector<nap::RenderableComponentInstance*> components_to_render
			{
				&mGnomonEntity->getComponent<RenderGnomonComponentInstance>()
			};

			// Render Gnomon
			mRenderService->renderObjects(*mRenderWindow, perp_cam, components_to_render);

			// Render GUI elements
			mGuiService->draw();

			// Stop render pass
			mRenderWindow->endRendering();

			// End recording
			mRenderService->endRecording();
		}

		// Proceed to next frame
		mRenderService->endFrame();
	}


	void lxcontrolApp::windowMessageReceived(WindowEventPtr windowEvent)
	{
		mRenderService->addEvent(std::move(windowEvent));
	}


	void lxcontrolApp::inputMessageReceived(InputEventPtr inputEvent)
	{
		if (inputEvent->get_type().is_derived_from(RTTI_OF(nap::KeyPressEvent)))
		{
			// If we pressed escape, quit the loop
			nap::KeyPressEvent* press_event = static_cast<nap::KeyPressEvent*>(inputEvent.get());
			if (press_event->mKey == nap::EKeyCode::KEY_ESCAPE)
				quit();

			// f is pressed, toggle full-screen
			if (press_event->mKey == nap::EKeyCode::KEY_f)
				mRenderWindow->toggleFullscreen();
		}
		// Add event, so it can be forwarded on update
		mInputService->addEvent(std::move(inputEvent));
	}


	int lxcontrolApp::shutdown()
	{
		return 0;
	}

}
