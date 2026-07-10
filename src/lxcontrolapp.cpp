#include "lxcontrolapp.h"

// External Includes
#include <utility/fileutils.h>
#include <nap/logger.h>
#include <inputrouter.h>
#include <rendergnomoncomponent.h>
#include <perspcameracomponent.h>
#include <midiinputcomponent.h>
#include <parameternumeric.h>
#include <imgui/imgui.h>
#include <mathutils.h>
#include <cstring>

// lx effect/modulator types (for RTTI_OF dispatch + casts)
#include <channelrole.h>
#include <effectparameter.h>
#include <adsrmodulator.h>
#include <lfomodulator.h>
#include <stepmodulator.h>

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::lxcontrolApp)
	RTTI_CONSTRUCTOR(nap::Core&)
RTTI_END_CLASS

namespace nap
{
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

		// The three per-fixture base-value parameter groups, drawn in the Fixtures tab. The fixtures
		// themselves self-register with lxcontrolService from their component init(), so there are no
		// hard-coded fixture lookups here anymore.
		for (const char* group_id : { "Strobe1_Params", "Strobe2_Params", "Strobe3_Params" })
		{
			auto group = mResourceManager->findObject<ParameterGroup>(group_id);
			if (!error.check(group != nullptr, "unable to find object with name: %s", group_id))
				return false;
			if (mFixtureParams1 == nullptr)		mFixtureParams1 = group;
			else if (mFixtureParams2 == nullptr)	mFixtureParams2 = group;
			else								mFixtureParams3 = group;
		}

		auto midi_monitor_entity = mScene->findEntity("MidiMonitorEntity");
		if (!error.check(midi_monitor_entity != nullptr, "unable to find entity with name: %s", "MidiMonitorEntity"))
			return false;

		auto* midi_input = midi_monitor_entity->findComponent<MidiInputComponentInstance>();
		if (!error.check(midi_input != nullptr, "MidiMonitorEntity is missing its MidiInputComponent"))
			return false;

		mMidiPort = mResourceManager->findObject<MidiInputPort>("MidiPort");
		if (!error.check(mMidiPort != nullptr, "unable to find object with name: %s", "MidiPort"))
			return false;

		if (!mLxControlService->setup(*midi_input, mMidiPort, error))
			return false;

		return true;
	}


	void lxcontrolApp::update(double deltaTime)
	{
		// Forward input events (recursively) to all input components in the default scene
		nap::DefaultInputRouter input_router(true);
		mInputService->processWindowEvents(*mRenderWindow, input_router, { &mScene->getRootEntity() });

		// Bind subsequent ImGui calls to our single window, then queue our GUI.
		mGuiService->selectWindow(mRenderWindow);
		drawMainUI();
	}


	// Draws a fixture's parameter group, rendering contiguous R/G/B channel triplets
	// (detected by display-name suffix, e.g. "Unit1 R"/"Unit1 G"/"Unit1 B") as a single
	// ImGui::ColorEdit3 swatch instead of three separate sliders.
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
				drawFixturesTab();
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


	void lxcontrolApp::drawFixturesTab()
	{
		ImGui::Text("Registered fixtures: %d", static_cast<int>(mLxControlService->getFixtures().size()));
		ImGui::Separator();

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
	}


	void lxcontrolApp::drawEffectsTab()
	{
		static const char* role_labels[] = { "Dimmer", "Strobe", "Red", "Green", "Blue", "ColorMacro", "SoundMode", "Generic" };
		static const char* shape_labels[] = { "Sine", "Ramp", "Triangle", "Square", "Pulse", "Gaussian" };

		ImGui::InputText("Name", mNewEffectName, sizeof(mNewEffectName));
		ImGui::SameLine();
		if (ImGui::Button("+ New Effect") && std::strlen(mNewEffectName) > 0)
		{
			mLxControlService->createEffect(mNewEffectName);
			mNewEffectName[0] = '\0';
		}
		ImGui::Separator();

		for (auto& effect : mLxControlService->getEffects())
		{
			ImGui::PushID(effect.get());
			bool open = ImGui::CollapsingHeader(effect->mName.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete"))
			{
				mLxControlService->removeEffect(effect.get());
				ImGui::PopID();
				break;
			}

			if (open)
			{
				// --- Parameters ---
				if (ImGui::Button("Add Float"))		mLxControlService->addEffectParameter(*effect.get(), RTTI_OF(lx::FloatParameter));
				ImGui::SameLine();
				if (ImGui::Button("Add Color"))		mLxControlService->addEffectParameter(*effect.get(), RTTI_OF(lx::ColorParameter));
				ImGui::SameLine();
				if (ImGui::Button("Add Toggle"))	mLxControlService->addEffectParameter(*effect.get(), RTTI_OF(lx::ToggleParameter));

				for (auto& p : effect->mParameters)
				{
					ImGui::PushID(p.get());
					if (auto* fp = rtti_cast<lx::FloatParameter>(p.get()))
					{
						int role = static_cast<int>(fp->mRole);
						ImGui::SetNextItemWidth(120);
						if (ImGui::Combo("Role", &role, role_labels, 8)) fp->mRole = static_cast<lx::EChannelRole>(role);
						ImGui::SameLine(); ImGui::SetNextItemWidth(140); ImGui::SliderFloat("Base", &fp->mValue, 0.0f, 1.0f);
					}
					else if (auto* cp = rtti_cast<lx::ColorParameter>(p.get()))
					{
						float col[3] = { cp->mRed, cp->mGreen, cp->mBlue };
						if (ImGui::ColorEdit3("Color", col)) { cp->mRed = col[0]; cp->mGreen = col[1]; cp->mBlue = col[2]; }
					}
					else if (auto* tp = rtti_cast<lx::ToggleParameter>(p.get()))
					{
						int role = static_cast<int>(tp->mRole);
						ImGui::SetNextItemWidth(120);
						if (ImGui::Combo("Role", &role, role_labels, 8)) tp->mRole = static_cast<lx::EChannelRole>(role);
						ImGui::SameLine(); ImGui::Checkbox("On", &tp->mValue);
					}
					ImGui::PopID();
				}

				ImGui::Separator();

				// --- Modulators ---
				int& tgt = mModTargetIndex[effect.get()];
				std::vector<const char*> plabels;
				for (auto& p : effect->mParameters) plabels.emplace_back(p->mName.c_str());
				if (!plabels.empty())
				{
					tgt = nap::math::clamp(tgt, 0, static_cast<int>(plabels.size()) - 1);
					ImGui::SetNextItemWidth(140);
					ImGui::Combo("Target", &tgt, plabels.data(), static_cast<int>(plabels.size()));
				}
				auto add_mod = [&](nap::rtti::TypeInfo type)
				{
					if (effect->mParameters.empty()) return;
					int i = nap::math::clamp(tgt, 0, static_cast<int>(effect->mParameters.size()) - 1);
					mLxControlService->addModulator(*effect.get(), type, effect->mParameters[i].get());
				};
				ImGui::SameLine(); if (ImGui::Button("Add ADSR")) add_mod(RTTI_OF(lx::AdsrModulator));
				ImGui::SameLine(); if (ImGui::Button("Add LFO"))  add_mod(RTTI_OF(lx::LfoModulator));
				ImGui::SameLine(); if (ImGui::Button("Add Step")) add_mod(RTTI_OF(lx::StepModulator));

				for (auto& m : effect->mModulators)
				{
					ImGui::PushID(m.get());
					ImGui::ProgressBar(m->value(), ImVec2(120, 0));
					ImGui::SameLine();
					ImGui::Text("-> %s", m->mTarget != nullptr ? m->mTarget->mName.c_str() : "(none)");
					ImGui::SameLine(); if (ImGui::SmallButton("Trigger")) m->onTrigger();
					ImGui::SameLine(); if (ImGui::SmallButton("Stop")) m->onStop();

					if (auto* adsr = rtti_cast<lx::AdsrModulator>(m.get()))
					{
						ImGui::SetNextItemWidth(80); ImGui::DragFloat("A", &adsr->mAttack, 0.01f, 0.0f, 10.0f); ImGui::SameLine();
						ImGui::SetNextItemWidth(80); ImGui::DragFloat("D", &adsr->mDecay, 0.01f, 0.0f, 10.0f); ImGui::SameLine();
						ImGui::SetNextItemWidth(80); ImGui::DragFloat("S", &adsr->mSustain, 0.01f, 0.0f, 1.0f); ImGui::SameLine();
						ImGui::SetNextItemWidth(80); ImGui::DragFloat("R", &adsr->mRelease, 0.01f, 0.0f, 10.0f);
					}
					else if (auto* lfo = rtti_cast<lx::LfoModulator>(m.get()))
					{
						int shape = static_cast<int>(lfo->mShape);
						ImGui::SetNextItemWidth(110);
						if (ImGui::Combo("Shape", &shape, shape_labels, 6)) lfo->mShape = static_cast<lx::ELfoShape>(shape);
						ImGui::SameLine(); ImGui::SetNextItemWidth(100); ImGui::DragFloat("Hz", &lfo->mFrequency, 0.05f, 0.0f, 30.0f);
					}
					else if (auto* step = rtti_cast<lx::StepModulator>(m.get()))
					{
						ImGui::SetNextItemWidth(100); ImGui::DragFloat("Rate", &step->mRate, 0.1f, 0.0f, 30.0f);
					}
					ImGui::DragFloat("Min", &m->mMin, 0.01f, 0.0f, 1.0f); ImGui::SameLine();
					ImGui::DragFloat("Max", &m->mMax, 0.01f, 0.0f, 1.0f);
					ImGui::Separator();
					ImGui::PopID();
				}
			}
			ImGui::PopID();
		}
	}


	void lxcontrolApp::drawMidiTab()
	{
		ImGui::Text("Connected MIDI input devices:");
		std::string port_names = mMidiPort->getPortNames();
		if (port_names.empty())
			ImGui::TextDisabled("No MIDI devices found");
		else
			ImGui::BulletText("%s", port_names.c_str());

		ImGui::Separator();
		ImGui::Text("Incoming messages:");
		ImGui::BeginChild("MidiLog", ImVec2(0, 240), true);
		for (const auto& line : mLxControlService->getMidiLog())
			ImGui::TextUnformatted(line.c_str());
		ImGui::EndChild();
	}


	void lxcontrolApp::render()
	{
		mRenderService->beginFrame();
		if (mRenderService->beginRecording(*mRenderWindow))
		{
			mRenderWindow->beginRendering();

			auto& perp_cam = mCameraEntity->getComponent<PerspCameraComponentInstance>();
			std::vector<nap::RenderableComponentInstance*> components_to_render
			{
				&mGnomonEntity->getComponent<RenderGnomonComponentInstance>()
			};
			mRenderService->renderObjects(*mRenderWindow, perp_cam, components_to_render);

			mGuiService->draw();

			mRenderWindow->endRendering();
			mRenderService->endRecording();
		}
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
			nap::KeyPressEvent* press_event = static_cast<nap::KeyPressEvent*>(inputEvent.get());
			if (press_event->mKey == nap::EKeyCode::KEY_ESCAPE)
				quit();
			if (press_event->mKey == nap::EKeyCode::KEY_f)
				mRenderWindow->toggleFullscreen();
		}
		mInputService->addEvent(std::move(inputEvent));
	}


	int lxcontrolApp::shutdown()
	{
		return 0;
	}
}
