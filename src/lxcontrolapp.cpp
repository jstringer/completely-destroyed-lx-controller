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
