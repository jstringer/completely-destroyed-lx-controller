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
#include "lxtheme.h"
#include "lxstyleguide.h"
#include "lxagent.h"
#include <cstring>
#include <algorithm>

// lx patch/modulator types (for RTTI_OF dispatch + casts)
#include <channelrole.h>
#include <fixturechannelcomponent.h>
#include <patchparameter.h>
#include <adsrmodulator.h>
#include <admodulator.h>
#include <lfomodulator.h>
#include <stepmodulator.h>
#include <chasemodulator.h>
#include <noisemodulator.h>
#include <trigger.h>
#include <control.h>
#include <midibinding.h>
#include <program.h>
#include <fixturecomponent.h>
#include <midievent.h>

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

		// Agent bridge (src/lxagent.h): take at most one queued command, BEFORE the GUI is queued, so a
		// click/tab lands in this very frame's widgets. No-op when nothing is driving the app.
		const auto cmd = lxagent::poll();
		if (!cmd.first.empty())
			handleAgentCommand(cmd.first, cmd.second);

		// Bind subsequent ImGui calls to our single window, then queue our GUI.
		mGuiService->selectWindow(mRenderWindow);
		drawMainUI();
		lxstyleguide::draw(&mShowStyleGuide);	// design-language test bed (own window)

		// Answer the bridge now that the frame is queued: HIT/MISS + state + every clickable label.
		lxagent::finish(agentStatus());
	}


	// Verbs the bridge itself can't serve (click/tab are consumed in lxagent::poll). Anything reachable
	// as a button is deliberately NOT duplicated here -- cue/load/fire/learn are `click` targets.
	void lxcontrolApp::handleAgentCommand(const std::string& verb, const std::string& arg)
	{
		if (verb == "mode")
		{
			mMode = arg == "perform" ? EUiMode::Perform : EUiMode::Edit;
			lxagent::hit();
		}
		else if (verb == "text")	// text <patch|control|group|program> <value> -- fills a creation form
		{
			const size_t sp = arg.find(' ');
			const std::string field = arg.substr(0, sp);
			const std::string value = sp == std::string::npos ? std::string() : arg.substr(sp + 1);
			char* dst = nullptr;
			size_t cap = 0;
			if (field == "patch")			{ dst = mNewPatchName;		cap = sizeof(mNewPatchName); }
			else if (field == "control")	{ dst = mNewControlName;	cap = sizeof(mNewControlName); }
			else if (field == "group")		{ dst = mNewControlGroup;	cap = sizeof(mNewControlGroup); }
			else if (field == "program")	{ dst = mNewProgramName;	cap = sizeof(mNewProgramName); }
			if (dst != nullptr)
			{
				std::strncpy(dst, value.c_str(), cap - 1);
				dst[cap - 1] = '\0';
				lxagent::hit();
			}
		}
		else if (verb == "styleguide")
		{
			mShowStyleGuide = arg != "off";
			lxagent::hit();
		}
		else if (verb == "resize")
		{
			int w = 0, h = 0;
			if (std::sscanf(arg.c_str(), "%d %d", &w, &h) == 2 && w > 0 && h > 0)
			{
				mRenderWindow->setSize({ w, h });
				lxagent::hit();
			}
		}
		else if (verb == "stopall")
		{
			mLxControlService->stopAll();
			lxagent::hit();
		}
		else if (verb == "state")
		{
			lxagent::hit();	// no-op: the ack is itself the state dump
		}
	}


	// The state lines appended to every bridge ack.
	std::string lxcontrolApp::agentStatus() const
	{
		lx::Program* active = mLxControlService->getActiveProgram();
		std::string s;
		s += "MODE: " + std::string(mMode == EUiMode::Perform ? "Perform" : "Edit") + "\n";
		s += "TAB: " + (mMode == EUiMode::Perform ? std::string("-") : mActiveTab) + "\n";
		s += "PROGRAM: " + (active != nullptr ? active->mID : std::string("<none>"))
			+ "  CUED: " + (mCuedProgram != nullptr ? mCuedProgram->mID : std::string("<none>")) + "\n";
		s += "VOICES: " + std::to_string(mLxControlService->activeVoiceCount()) + "\n";
		return s;
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
		lxtheme::applyStyle();	// own the full terminal/luminous style each frame (wins over the SDK scheme)

		// Full-bleed chassis: pin the main window to the whole viewport (no titlebar/move/resize) so the
		// UI fills the OS window instead of floating with black margins. The Style Guide stays a separate
		// opt-in overlay (off by default).
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
		const ImGuiWindowFlags chassis_flags =
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;
		ImGui::Begin("lxcontrol", nullptr, chassis_flags);

		drawLiveBar();
		ImGui::Separator();

		if (mMode == EUiMode::Perform)
		{
			drawPerformGrid();
			ImGui::End();
			return;
		}

		// Edit mode: the three authoring surfaces RIG / PROGRAMS / CONTROLS.
		if (ImGui::BeginTabBar("MainTabs"))
		{
			if (ImGui::BeginTabItem("RIG", nullptr, lxagent::tabFlags("RIG")))
			{
				mActiveTab = "RIG";
				drawRigTab();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("PROGRAMS", nullptr, lxagent::tabFlags("PROGRAMS")))
			{
				mActiveTab = "PROGRAMS";
				// 3-panel workspace: programs list | routing+automatic+output | shared patch editor.
				lx::Program* cued = mCuedProgram != nullptr ? mCuedProgram : mLxControlService->getActiveProgram();
				ImGui::BeginChild("prog_list", ImVec2(190, 0), true);
				drawProgramsListPanel();
				ImGui::EndChild();
				ImGui::SameLine();
				ImGui::BeginChild("prog_route", ImVec2(ImGui::GetContentRegionAvail().x - 456.0f, 0), true);
				drawRoutingPanel(cued);
				ImGui::EndChild();
				ImGui::SameLine();
				ImGui::BeginChild("prog_patch", ImVec2(440, 0), true);
				ImGui::TextColored(lxtheme::accent(), "PATCH EDITOR");
				ImGui::Separator();
				drawPatchesTab();
				ImGui::EndChild();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("CONTROLS", nullptr, lxagent::tabFlags("CONTROLS")))
			{
				mActiveTab = "CONTROLS";
				drawControlsTab();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();
	}


	// Steps mCuedProgram to the previous/next program in the list (cue-select, does NOT load).
	void lxcontrolApp::cueProgram(int dir)
	{
		const auto& progs = mLxControlService->getPrograms();
		if (progs.empty()) { mCuedProgram = nullptr; return; }
		int idx = 0;
		for (int i = 0; i < static_cast<int>(progs.size()); ++i)
			if (progs[i].get() == mCuedProgram) { idx = i; break; }
		idx = (idx + dir + static_cast<int>(progs.size())) % static_cast<int>(progs.size());
		mCuedProgram = progs[idx].get();
	}

	void lxcontrolApp::drawLiveBar()
	{
		lx::Program* active = mLxControlService->getActiveProgram();
		const size_t voices = mLxControlService->activeVoiceCount();

		// Default the cue to the loaded program the first time / when it points at nothing.
		if (mCuedProgram == nullptr)
			mCuedProgram = active;

		// --- Scene selector: [◀] LOADED <name> [▶], + Load when a different program is cued ---
		if (lxagent::SmallButton("<")) cueProgram(-1);
		ImGui::SameLine();
		lxtheme::Chip("LOADED");
		ImGui::SameLine();
		if (active != nullptr)
			ImGui::TextColored(lxtheme::live(), "%s", active->mName.c_str());
		else
			ImGui::TextDisabled("- none -");
		ImGui::SameLine();
		if (lxagent::SmallButton(">")) cueProgram(+1);

		if (mCuedProgram != nullptr && mCuedProgram != active)
		{
			ImGui::SameLine(0.0f, 14.0f);
			ImGui::TextDisabled("cued:");
			ImGui::SameLine();
			ImGui::TextColored(lxtheme::accent2(), "%s", mCuedProgram->mName.c_str());
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, lxtheme::accent2());
			if (lxagent::SmallButton("Load >")) mLxControlService->loadProgram(mCuedProgram);
			ImGui::PopStyleColor();
		}

		// --- Right cluster: All Stop | output state | MIDI activity | mode | style guide ---
		ImGui::SameLine(0.0f, 28.0f);
		if (lxtheme::DangerButton("# All Stop"))
			mLxControlService->stopAll();

		// Honest output state (never a phantom "on").
		ImGui::SameLine(0.0f, 20.0f);
		if (voices > 0)
		{
			lxtheme::StateDot(lxtheme::live(), true);
			ImGui::SameLine();
			ImGui::TextColored(lxtheme::live2(), "Output - %d held", static_cast<int>(voices));
		}
		else
		{
			lxtheme::StateDot(lxtheme::rgb(0x3a2a12));
			ImGui::SameLine();
			ImGui::TextDisabled(active != nullptr ? "loaded - idle" : "dark");
		}

		// MIDI activity: time since the last message (not a fake "connected" light).
		ImGui::SameLine(0.0f, 20.0f);
		const int ctr = mLxControlService->getMidiEventCounter();
		if (ctr != mLastMidiCounter) { mLastMidiCounter = ctr; mLastMidiSeen = ImGui::GetTime(); }
		if (mLastMidiSeen < 0.0)
			ImGui::TextDisabled("MIDI -");
		else
		{
			ImGui::TextDisabled("MIDI");
			ImGui::SameLine();
			ImGui::TextColored(lxtheme::accent2(), "%.1fs", ImGui::GetTime() - mLastMidiSeen);
		}

		// Perform/Edit toggle.
		ImGui::SameLine(0.0f, 24.0f);
		const bool perform = (mMode == EUiMode::Perform);
		if (perform) { ImGui::PushStyleColor(ImGuiCol_Button, lxtheme::rgb(0x2dd4bf, 0.16f)); ImGui::PushStyleColor(ImGuiCol_Text, lxtheme::accent2()); }
		if (lxagent::Button(perform ? "< EDIT" : "PERFORM >"))
			mMode = perform ? EUiMode::Edit : EUiMode::Perform;
		if (perform) ImGui::PopStyleColor(2);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Toggle Perform (play-only) / Edit (authoring)");

		ImGui::SameLine(0.0f, 16.0f);
		ImGui::Checkbox("Style Guide", &mShowStyleGuide);
	}


	void lxcontrolApp::drawPerformGrid()
	{
		lx::Program* active = mLxControlService->getActiveProgram();
		if (active != nullptr)
			lxtheme::SectionHeader((std::string("Perform - ") + active->mName).c_str());
		else
			lxtheme::SectionHeader("Perform");

		if (active == nullptr)
		{
			ImGui::Spacing();
			ImGui::TextDisabled("No program loaded. Cue one in the bar and press Load to perform it.");
			return;
		}

		const auto& controls = mLxControlService->getControls();
		if (controls.empty())
		{
			ImGui::Spacing();
			ImGui::TextDisabled("No controls yet. Create some in Edit mode (CONTROLS).");
			return;
		}

		// Play-only pad grid (mockup .bigpad, aspect ~1.5). One pad per Control, firing its mapped
		// Trigger; lit=gold when its trigger is active; unbound dimmed. All-Stop lives in the bar.
		const int cols = 4;
		const float avail = ImGui::GetContentRegionAvail().x;
		const float pad_w = (avail - ImGui::GetStyle().ItemSpacing.x * (cols - 1)) / cols;
		const float pad_h = pad_w / 1.5f;
		int col = 0;
		for (auto& c : controls)
		{
			lx::Trigger* trig = mLxControlService->getControlMapping(*active, *c.get());
			const bool bound = (trig != nullptr);
			const bool live = bound && mLxControlService->isTriggerActive(*trig);

			// Sublabel: the patch(es) this control fires, from its trigger's first binding.
			std::string fx;
			if (bound && !trig->mBindings.empty() && trig->mBindings[0].mPatch != nullptr)
				fx = trig->mBindings[0].mPatch->mName + "  [" + std::to_string(trig->mBindings[0].mFixtureNames.size()) + " fx]";
			else if (bound)
				fx = "no patch";
			else
				fx = "unbound";

			ImGui::PushID(c.get());
			if (live)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, lxtheme::rgb(0xf5b301, 0.20f));
				ImGui::PushStyleColor(ImGuiCol_Border, lxtheme::live());
			}
			const bool dis = lxtheme::PushDisabled(!bound);

			// The button carries the folded "name fx" label (bridge-addressable) but its own text is
			// hidden; we draw the cue name + muted fx sublabel top-left ourselves (mockup .bigpad),
			// since ImGui::Button centers multi-line text.
			const ImVec2 p = ImGui::GetCursorScreenPos();
			const std::string label = c->mName + "\n" + fx;
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));
			if (lxagent::Button(label.c_str(), ImVec2(pad_w, pad_h)) && bound)
				mLxControlService->fireTrigger(*trig);
			ImGui::PopStyleColor();

			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->AddText(ImVec2(p.x + 12, p.y + 12), ImGui::ColorConvertFloat4ToU32(live ? lxtheme::live2() : lxtheme::text()), c->mName.c_str());
			dl->AddText(ImVec2(p.x + 12, p.y + 12 + ImGui::GetTextLineHeight() + 4),
				ImGui::ColorConvertFloat4ToU32(bound ? lxtheme::muted() : lxtheme::live2()), fx.c_str());

			lxtheme::PopDisabled(dis);
			if (live) ImGui::PopStyleColor(2);
			if (!bound && ImGui::IsItemHovered())
				ImGui::SetTooltip("Unbound in this program - route it in Edit / PROGRAMS");
			ImGui::PopID();

			if (++col % cols != 0)
				ImGui::SameLine();
		}
		ImGui::NewLine();
		ImGui::TextDisabled("Play-only. All Stop and the output readout stay in the top bar. Toggle EDIT to author.");
	}


	// First channel matching `role` (0..1 resolved output), or 0 if the fixture has no such channel.
	static float roleOutput(lx::FixtureComponentInstance& fx, lx::EChannelRole role)
	{
		for (auto* ch : fx.getChannels())
			if (ch->getRole() == role)
				return ch->resolveValue();
		return 0.0f;
	}


	// A tall labeled display fader (read-only) showing 0..1 output, with value below. Mockup .vf.
	static void tallFader(const char* label, float v01, const ImVec4& fill)
	{
		ImGui::BeginGroup();
		lxtheme::Fader(label, v01, ImVec2(24, 130), fill);
		ImGui::PushStyleColor(ImGuiCol_Text, lxtheme::muted());
		ImGui::Text("%s", label);
		ImGui::PopStyleColor();
		if (v01 > 0.001f)
			ImGui::Text("%d", static_cast<int>(v01 * 255.0f + 0.5f));
		else
			ImGui::TextDisabled("0");
		ImGui::EndGroup();
	}


	void lxcontrolApp::drawFixtureOutputStrip(lx::FixtureComponentInstance& fx)
	{
		// Dimmer + Strobe tall faders (real post-arbitration output).
		tallFader("Dimmer", roleOutput(fx, lx::EChannelRole::Dimmer), lxtheme::live());
		ImGui::SameLine(0.0f, 20.0f);
		tallFader("Strobe", roleOutput(fx, lx::EChannelRole::Strobe), lxtheme::accent());

		// Color - 6 units.
		ImGui::PushStyleColor(ImGuiCol_Text, lxtheme::muted());
		ImGui::Text("Color - 6 units");
		ImGui::PopStyleColor();
		int shown = 0;
		for (int unit = 1; unit <= 6; ++unit)
		{
			float rgb[3] = { 0, 0, 0 };
			bool present = false;
			for (auto* ch : fx.getChannels())
			{
				if (ch->getUnitIndex() != unit) continue;
				if (ch->getRole() == lx::EChannelRole::Red)   { rgb[0] = ch->resolveValue(); present = true; }
				if (ch->getRole() == lx::EChannelRole::Green) { rgb[1] = ch->resolveValue(); present = true; }
				if (ch->getRole() == lx::EChannelRole::Blue)  { rgb[2] = ch->resolveValue(); present = true; }
			}
			if (!present) continue;
			ImGui::PushID(unit);
			lxtheme::Swatch("##u", rgb, 26.0f);
			ImGui::PopID();
			if (shown < 5) ImGui::SameLine();
			shown++;
		}
		if (shown == 0) ImGui::TextDisabled("(no RGB units)");

		// Mode row (Preset color / sound), shown as chips reflecting current output.
		ImGui::PushStyleColor(ImGuiCol_Text, lxtheme::muted());
		ImGui::Text("Mode");
		ImGui::PopStyleColor();
		lxtheme::Chip("Preset");
		ImGui::SameLine();
		lxtheme::Chip(roleOutput(fx, lx::EChannelRole::SoundMode) > 0.02f ? "Sound on" : "Sound off");
	}


	void lxcontrolApp::drawRigTab()
	{
		auto fixtures = mLxControlService->getFixturesPhysicalOrder();

		// --- Art-Net info bar: topology is start-time-only (read-only); derived from the live rig. ---
		ImGui::BeginChild("artnet", ImVec2(0, 52), true);
		lxtheme::Chip("Art-Net output");
		ImGui::SameLine(0.0f, 18.0f); ImGui::TextDisabled("Universe [lock]"); ImGui::SameLine(); ImGui::Text("0");
		ImGui::SameLine(0.0f, 18.0f); ImGui::TextDisabled("Send to [lock]"); ImGui::SameLine(); ImGui::Text("auto (broadcast)");
		ImGui::SameLine(0.0f, 18.0f); ImGui::TextDisabled("Start channels [lock]"); ImGui::SameLine();
		{
			std::string chans;
			for (size_t i = 0; i < fixtures.size(); ++i)
				chans += (i ? " . " : "") + std::to_string(fixtures[i]->getStartChannel());
			ImGui::Text("%s", chans.empty() ? "-" : chans.c_str());
		}
		ImGui::SameLine(0.0f, 18.0f); ImGui::TextColored(lxtheme::accent2(), "Refresh"); ImGui::SameLine(); ImGui::Text("44 Hz");
		ImGui::SameLine(0.0f, 18.0f); ImGui::TextDisabled("[lock] topology (universe / mode / fixtures) set on startup - restart to change");
		ImGui::EndChild();

		lxtheme::SectionHeader("Fixtures  -  Eurolite Strobe 540 . 22ch");

		// Physical (StartChannel) order aligns with Strobe1/2/3 param groups (StartChannels 0/22/44).
		const char* fixture_names[3] = { "Strobe 1", "Strobe 2", "Strobe 3" };
		ParameterGroup* fixture_groups[3] = { mFixtureParams1.get(), mFixtureParams2.get(), mFixtureParams3.get() };
		for (int i = 0; i < 3; i++)
		{
			if (i > 0)
				ImGui::SameLine();
			// Content-height card (not a full-height empty well). Scrolls only if "Manual base values"
			// is expanded, which is the rare case.
			ImGui::BeginChild(fixture_names[i], ImVec2(320, 362), true);

			lx::FixtureComponentInstance* fx = (i < static_cast<int>(fixtures.size())) ? fixtures[i] : nullptr;

			// Header: name + live output LED (lit if anything is emitting on this fixture).
			ImGui::TextColored(lxtheme::text(), "%s", fixture_names[i]);
			if (fx != nullptr)
			{
				bool any_out = false;
				for (auto* ch : fx->getChannels())
					if (ch->resolveValue() > 0.02f) { any_out = true; break; }
				ImGui::SameLine();
				ImGui::TextDisabled("%s @ ch %d", fx->getDisplayName().c_str(), fx->getStartChannel());
				ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 16.0f);
				lxtheme::OutputLed(any_out, lxtheme::live());
			}
			ImGui::Separator();

			if (fx != nullptr)
				drawFixtureOutputStrip(*fx);

			// Manual base values kept available but out of the way (mockup RIG is a monitor).
			if (ImGui::CollapsingHeader("Manual base values"))
				drawFixtureParamGroup(*fixture_groups[i]);

			ImGui::EndChild();
		}
	}


	std::string lxcontrolApp::describePatchVoice(lx::Patch* patch, int voice)
	{
		auto ordered_fixtures = mLxControlService->getFixturesPhysicalOrder();
		for (auto& trigger : mLxControlService->getTriggers())
		{
			for (auto& binding : trigger->mBindings)
			{
				if (binding.mPatch.get() != patch)
					continue;
				int idx = 0;
				for (auto* f : ordered_fixtures)
				{
					if (std::find(binding.mFixtureNames.begin(), binding.mFixtureNames.end(), f->getEntityID()) == binding.mFixtureNames.end())
						continue;
					if (idx == voice)
						return f->getDisplayName();
					++idx;
				}
			}
		}
		return "Voice " + std::to_string(voice);
	}


	void lxcontrolApp::drawPatchesTab()
	{
		static const char* role_labels[] = { "Dimmer", "Strobe", "Red", "Green", "Blue", "ColorMacro", "SoundMode", "Generic" };
		static const char* shape_labels[] = { "Sine", "Ramp", "Triangle", "Square", "Pulse", "Gaussian" };
		static const char* blend_labels[]    = { "Replace", "Multiply", "Add" };
		static const char* lfo_mode_labels[] = { "Loop", "OneShot", "LoopRetrigger" };
		static const char* ad_mode_labels[]  = { "OneShot", "LoopWhileSustained" };

		// New patch.
		ImGui::SetNextItemWidth(-116.0f);
		ImGui::InputText("##patchname", mNewPatchName, sizeof(mNewPatchName));
		ImGui::SameLine();
		if (lxagent::Button("+ New Patch") && std::strlen(mNewPatchName) > 0)
		{
			mLxControlService->createPatch(mNewPatchName);
			mNewPatchName[0] = '\0';
		}
		ImGui::Separator();

		if (mLxControlService->getPatches().empty())
			ImGui::TextDisabled("No patches yet. A patch is a light-voice: parameters + modulators.");

		for (auto& patch : mLxControlService->getPatches())
		{
			ImGui::PushID(patch.get());
			bool open = ImGui::CollapsingHeader(patch->mName.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

			int used_by = 0;
			for (auto& trig : mLxControlService->getTriggers())
				for (auto& b : trig->mBindings)
					if (b.mPatch.get() == patch.get()) { used_by++; break; }

			ImGui::SameLine();
			if (lxtheme::DangerButton("Delete"))
				ImGui::OpenPopup("confirm_del_patch");
			if (ImGui::BeginPopup("confirm_del_patch"))
			{
				if (used_by > 0)
					ImGui::TextColored(lxtheme::danger(), "Used by %d trigger binding%s.", used_by, used_by == 1 ? "" : "s");
				ImGui::Text("Delete patch \"%s\"?", patch->mName.c_str());
				if (lxtheme::DangerButton("Delete"))
				{
					mLxControlService->removePatch(patch.get());
					ImGui::CloseCurrentPopup();
					ImGui::EndPopup();
					ImGui::PopID();
					break;
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
			}

			if (open)
			{
				// used-by N + Fork (deep-copy deferred).
				if (used_by > 0)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, lxtheme::live2());
					ImGui::Text("Used by %d trigger%s - edits apply to all", used_by, used_by == 1 ? "" : "s");
					ImGui::PopStyleColor();
					ImGui::SameLine();
					bool fdis = lxtheme::PushDisabled(true);
					lxagent::SmallButton("Fork");
					lxtheme::PopDisabled(fdis);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Deep-copy for this program - not yet implemented");
				}

				// Spread + voice count.
				static const char* target_mode_labels[] = { "Single Fixture", "Multiple Fixtures" };
				int mode = static_cast<int>(patch->mTargetMode);
				ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("Spread"); ImGui::SameLine();
				ImGui::SetNextItemWidth(160);
				if (ImGui::Combo("##spread", &mode, target_mode_labels, 2))
					mLxControlService->setPatchTargetMode(*patch.get(), static_cast<lx::EPatchTargetMode>(mode));
				if (mode == static_cast<int>(lx::EPatchTargetMode::Multiple))
				{
					ImGui::SameLine();
					std::string vc = std::to_string(patch->mFixtureCount) + (patch->mFixtureCount == 1 ? " voice" : " voices");
					lxtheme::Chip(vc.c_str());
				}

				// Playhead preview from the first modulator's live history.
				if (!patch->mModulators.empty())
				{
					auto& hist = mModHistory[patch->mModulators[0]->mID];
					hist.push_back(patch->mModulators[0]->mSink != nullptr ? patch->mModulators[0]->mSink->mValue : 0.0f);
					if (hist.size() > 160) hist.erase(hist.begin());
					lxtheme::PlayheadPreview(hist.data(), static_cast<int>(hist.size()), ImVec2(-1.0f, 50.0f));
				}

				// --- SOURCE zone: parameters, one per row ---
				lxtheme::SectionHeader("Source");
				for (auto& p : patch->mParameters)
				{
					ImGui::PushID(p.get());
					if (auto* fp = rtti_cast<lx::FloatParameter>(p.get()))
					{
						int role = static_cast<int>(fp->mRole);
						ImGui::SetNextItemWidth(120);
						if (ImGui::Combo("##role", &role, role_labels, 8)) fp->mRole = static_cast<lx::EChannelRole>(role);
						ImGui::SameLine(); ImGui::SetNextItemWidth(-1.0f);
						ImGui::SliderFloat("##base", &fp->mValue, 0.0f, 1.0f);
					}
					else if (auto* cp = rtti_cast<lx::ColorParameter>(p.get()))
					{
						float col[3] = { cp->mRed, cp->mGreen, cp->mBlue };
						if (ImGui::ColorEdit3("Color", col, ImGuiColorEditFlags_NoInputs))
						{ cp->mRed = col[0]; cp->mGreen = col[1]; cp->mBlue = col[2]; }
						ImGui::SameLine();
						ImGui::TextDisabled("%d . %d . %d", static_cast<int>(col[0] * 255), static_cast<int>(col[1] * 255), static_cast<int>(col[2] * 255));
					}
					else if (auto* tp = rtti_cast<lx::ToggleParameter>(p.get()))
					{
						int role = static_cast<int>(tp->mRole);
						ImGui::SetNextItemWidth(120);
						if (ImGui::Combo("##trole", &role, role_labels, 8)) tp->mRole = static_cast<lx::EChannelRole>(role);
						ImGui::SameLine(); ImGui::Checkbox("On", &tp->mValue);
					}
					ImGui::PopID();
				}
				if (lxagent::Button("+ Float")) mLxControlService->addPatchParameter(*patch.get(), RTTI_OF(lx::FloatParameter));
				ImGui::SameLine(); if (lxagent::Button("+ Color")) mLxControlService->addPatchParameter(*patch.get(), RTTI_OF(lx::ColorParameter));
				ImGui::SameLine(); if (lxagent::Button("+ Toggle")) mLxControlService->addPatchParameter(*patch.get(), RTTI_OF(lx::ToggleParameter));

				// --- MODULATION zone ---
				lxtheme::SectionHeader("Modulation");
				int& tgt = mModTargetIndex[patch->mID];
				std::vector<const char*> plabels;
				for (auto& p : patch->mParameters) plabels.emplace_back(p->mName.c_str());
				auto add_mod = [&](nap::rtti::TypeInfo type)
				{
					if (patch->mParameters.empty()) return;
					int i = nap::math::clamp(tgt, 0, static_cast<int>(patch->mParameters.size()) - 1);
					mLxControlService->addModulator(*patch.get(), type, patch->mParameters[i].get());
				};
				if (!plabels.empty())
				{
					tgt = nap::math::clamp(tgt, 0, static_cast<int>(plabels.size()) - 1);
					ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("drives"); ImGui::SameLine();
					ImGui::SetNextItemWidth(140);
					ImGui::Combo("##modtarget", &tgt, plabels.data(), static_cast<int>(plabels.size()));
					if (lxagent::Button("ADSR")) add_mod(RTTI_OF(lx::AdsrModulator));
					ImGui::SameLine(); if (lxagent::Button("AD"))   add_mod(RTTI_OF(lx::AdModulator));
					ImGui::SameLine(); if (lxagent::Button("LFO"))  add_mod(RTTI_OF(lx::LfoModulator));
					ImGui::SameLine(); if (lxagent::Button("Step")) add_mod(RTTI_OF(lx::StepModulator));
					ImGui::SameLine(); if (lxagent::Button("Chase")) add_mod(RTTI_OF(lx::ChaseModulator));
					ImGui::SameLine(); if (lxagent::Button("Noise")) add_mod(RTTI_OF(lx::NoiseModulator));
				}
				else
				{
					ImGui::TextDisabled("add a parameter first, then modulators can drive it");
				}

				for (auto& m : patch->mModulators)
				{
					ImGui::PushID(m.get());

					// Card header: kind (violet) + target + Trigger/Stop.
					const char* kind = "MOD";
					if (rtti_cast<lx::AdsrModulator>(m.get())) kind = "ADSR";
					else if (rtti_cast<lx::AdModulator>(m.get())) kind = "AD";
					else if (rtti_cast<lx::LfoModulator>(m.get())) kind = "LFO";
					else if (rtti_cast<lx::StepModulator>(m.get())) kind = "STEP";
					else if (rtti_cast<lx::ChaseModulator>(m.get())) kind = "CHASE";
					else if (rtti_cast<lx::NoiseModulator>(m.get())) kind = "NOISE";
					ImGui::Separator();
					ImGui::TextColored(lxtheme::mod(), "%s", kind);
					ImGui::SameLine();
					ImGui::TextColored(lxtheme::mod2(), "> drives %s", m->mTarget != nullptr ? m->mTarget->mName.c_str() : "(none)");
					ImGui::SameLine(); if (lxagent::SmallButton("Trigger")) m->onTrigger();
					ImGui::SameLine(); if (lxagent::SmallButton("Stop")) m->onStop();

					bool is_voice_mod = rtti_cast<lx::ChaseModulator>(m.get()) != nullptr || rtti_cast<lx::NoiseModulator>(m.get()) != nullptr;
					if (is_voice_mod)
					{
						int voices = patch->mTargetMode == lx::EPatchTargetMode::Multiple ?
							nap::math::clamp(patch->mFixtureCount, 1, 32) : 1;
						for (int s = 0; s < voices; ++s)
						{
							ImGui::PushID(s);
							ImGui::ProgressBar(m->valueForVoice(s), ImVec2(90, 0), describePatchVoice(patch.get(), s).c_str());
							ImGui::PopID();
							if (s + 1 < voices) ImGui::SameLine();
						}
					}
					else
					{
						auto& hist = mModHistory[m->mID];
						hist.push_back(m->mSink != nullptr ? m->mSink->mValue : 0.0f);
						if (hist.size() > 120) hist.erase(hist.begin());
						lxtheme::ModPlot("##plot", hist.data(), static_cast<int>(hist.size()), ImVec2(-1.0f, 40.0f));
					}

					auto regen = [&]() { m->generateCurve(*mLxControlService); };

					if (auto* adsr = rtti_cast<lx::AdsrModulator>(m.get()))
					{
						bool ch = false;
						ImGui::SetNextItemWidth(64); ch |= ImGui::DragFloat("A", &adsr->mAttack, 0.01f, 0.0f, 10.0f); ImGui::SameLine();
						ImGui::SetNextItemWidth(64); ch |= ImGui::DragFloat("D", &adsr->mDecay, 0.01f, 0.0f, 10.0f); ImGui::SameLine();
						ImGui::SetNextItemWidth(64); ch |= ImGui::DragFloat("S", &adsr->mSustain, 0.01f, 0.0f, 1.0f); ImGui::SameLine();
						ImGui::SetNextItemWidth(64); ch |= ImGui::DragFloat("R", &adsr->mRelease, 0.01f, 0.0f, 10.0f); ImGui::SameLine();
						ImGui::Checkbox("Loop", &adsr->mLoop);
						if (ch) regen();
					}
					else if (auto* ad = rtti_cast<lx::AdModulator>(m.get()))
					{
						bool ch = false;
						ImGui::SetNextItemWidth(64); ch |= ImGui::DragFloat("A", &ad->mAttack, 0.01f, 0.0f, 10.0f); ImGui::SameLine();
						ImGui::SetNextItemWidth(64); ch |= ImGui::DragFloat("D", &ad->mDecay, 0.01f, 0.0f, 10.0f); ImGui::SameLine();
						int amode = static_cast<int>(ad->mMode);
						ImGui::SetNextItemWidth(150);
						if (ImGui::Combo("Mode##ad", &amode, ad_mode_labels, 2)) ad->mMode = static_cast<lx::EAdMode>(amode);
						if (ch) regen();
					}
					else if (auto* lfo = rtti_cast<lx::LfoModulator>(m.get()))
					{
						int shape = static_cast<int>(lfo->mShape);
						ImGui::SetNextItemWidth(110);
						if (ImGui::Combo("Shape", &shape, shape_labels, 6)) { lfo->mShape = static_cast<lx::ELfoShape>(shape); regen(); }
						ImGui::SameLine(); ImGui::SetNextItemWidth(80);
						if (ImGui::DragFloat("Hz", &lfo->mFrequency, 0.05f, 0.0f, 30.0f) && lfo->mPlayer != nullptr)
							lfo->mPlayer->setPlaybackSpeed(lfo->mFrequency);
						int lmode = static_cast<int>(lfo->mMode);
						ImGui::SameLine(); ImGui::SetNextItemWidth(120);
						if (ImGui::Combo("Mode##lfo", &lmode, lfo_mode_labels, 3)) lfo->mMode = static_cast<lx::ELfoMode>(lmode);
					}
					else if (auto* step = rtti_cast<lx::StepModulator>(m.get()))
					{
						bool ch = false;
						ImGui::SetNextItemWidth(80); ch |= ImGui::DragFloat("Rate", &step->mRate, 0.1f, 0.1f, 30.0f); ImGui::SameLine();
						ch |= ImGui::Checkbox("Glide", &step->mGlide);
						if (ch) regen();
					}
					else if (auto* chase = rtti_cast<lx::ChaseModulator>(m.get()))
					{
						ImGui::SetNextItemWidth(80); ImGui::DragFloat("Rate", &chase->mRate, 0.05f, 0.0f, 30.0f); ImGui::SameLine();
						ImGui::SetNextItemWidth(80); ImGui::SliderFloat("PulseWidth", &chase->mPulseWidth, 0.01f, 1.0f); ImGui::SameLine();
						ImGui::Checkbox("Glide", &chase->mGlide);
					}
					else if (auto* noise = rtti_cast<lx::NoiseModulator>(m.get()))
					{
						ImGui::SetNextItemWidth(80); ImGui::DragFloat("Rate", &noise->mRate, 0.05f, 0.0f, 30.0f); ImGui::SameLine();
						ImGui::SetNextItemWidth(100); ImGui::SliderFloat("Smoothing", &noise->mSmoothing, 0.0f, 1.0f); ImGui::SameLine();
						ImGui::SetNextItemWidth(80); ImGui::InputInt("Seed", &noise->mSeed);
					}

					int blend = static_cast<int>(m->mBlend);
					ImGui::SetNextItemWidth(100);
					if (ImGui::Combo("Blend", &blend, blend_labels, 3)) m->mBlend = static_cast<lx::EModulatorBlend>(blend);
					ImGui::SameLine(); ImGui::SetNextItemWidth(80); ImGui::DragFloat("Min", &m->mMin, 0.01f, 0.0f, 1.0f);
					ImGui::SameLine(); ImGui::SetNextItemWidth(80); ImGui::DragFloat("Max", &m->mMax, 0.01f, 0.0f, 1.0f);
					ImGui::PopID();
				}
			}
			ImGui::PopID();
		}
	}


	void lxcontrolApp::drawTriggerBindingsEditor(lx::Trigger& trigger)
	{
		const auto& patches = mLxControlService->getPatches();
		// Physical rig order (by StartChannel), matching exactly the order lxcontrolService::fireTrigger
		// assigns Chase/Noise fixture voices in -- so the voice number previewed below is what a fixture
		// will actually get, not a guess based on checkbox/registration order.
		auto ordered_fixtures = mLxControlService->getFixturesPhysicalOrder();

		auto displayNameFor = [&](const std::string& entityID) -> std::string
		{
			for (auto* f : ordered_fixtures)
				if (f->getEntityID() == entityID)
					return f->getDisplayName();
			return entityID;	// fixture no longer in the rig; show the raw id rather than dropping it silently
		};

		if (ImGui::TreeNode("Bindings"))
		{
			int bi = 0;
			bool removed = false;
			for (auto& b : trigger.mBindings)
			{
				ImGui::PushID(bi);
				std::string fx;
				for (auto& f : b.mFixtureNames) { fx += displayNameFor(f); fx += " "; }
				ImGui::BulletText("%s -> %s", b.mPatch != nullptr ? b.mPatch->mName.c_str() : "(none)", fx.c_str());
				ImGui::SameLine();
				if (lxagent::SmallButton("Remove"))
				{
					auto bindings = trigger.mBindings;
					bindings.erase(bindings.begin() + bi);
					mLxControlService->setTriggerBindings(trigger, bindings);
					removed = true;
				}
				ImGui::PopID();
				if (removed) break;
				bi++;
			}

			if (!removed)
			{
				if (!patches.empty())
				{
					int& eidx = mBindPatchIdx[trigger.mID];
					std::vector<const char*> elabels;
					for (auto& e : patches) elabels.emplace_back(e->mName.c_str());
					eidx = nap::math::clamp(eidx, 0, static_cast<int>(elabels.size()) - 1);
					ImGui::SetNextItemWidth(160);
					ImGui::Combo("Patch", &eidx, elabels.data(), static_cast<int>(elabels.size()));

					auto& sel = mBindFixtures[trigger.mID];
					bool multiple = patches[eidx]->mTargetMode == lx::EPatchTargetMode::Multiple;
					if (multiple)
					{
						ImGui::SameLine();
						ImGui::TextDisabled("(%d selected -- each gets its own animated voice, any number is fine)", static_cast<int>(sel.size()));
					}

					// One checkbox per fixture, in physical rig order. For a Multiple-mode patch, show the
					// voice each checked fixture will actually resolve to (computed the same way fireTrigger
					// does: position among checked fixtures in physical order) -- this is a preview, not a
					// separate authored number, so there's nothing here that can fall out of sync.
					int voice_preview = 0;
					for (auto* f : ordered_fixtures)
					{
						ImGui::PushID(f);
						std::string eid = f->getEntityID();
						bool checked = sel.count(eid) > 0;
						std::string label = f->getDisplayName();
						if (multiple && checked)
						{
							label += " [voice " + std::to_string(voice_preview) + "]";
							++voice_preview;
						}
						if (ImGui::Checkbox(label.c_str(), &checked))
						{
							if (checked) sel.insert(eid); else sel.erase(eid);
						}
						ImGui::PopID();
					}
					if (lxagent::Button("+ Add Binding") && !sel.empty())
					{
						auto bindings = trigger.mBindings;
						lx::PatchFixtureBinding nb;
						nb.mPatch = patches[eidx].get();
						for (auto& s : sel) nb.mFixtureNames.emplace_back(s);
						bindings.emplace_back(nb);
						mLxControlService->setTriggerBindings(trigger, bindings);
						sel.clear();
					}
				}
				else
				{
					ImGui::TextDisabled("Create an patch first");
				}
			}
			ImGui::TreePop();
		}
	}


	void lxcontrolApp::drawProgramsListPanel()
	{
		lxtheme::SectionHeader("Programs");
		lx::Program* active = mLxControlService->getActiveProgram();
		const auto& progs = mLxControlService->getPrograms();
		if (progs.empty())
			ImGui::TextDisabled("none yet");

		for (auto& prog : progs)
		{
			ImGui::PushID(prog.get());
			bool is_active = (prog.get() == active);
			bool is_sel = (prog.get() == mCuedProgram);
			if (is_active) { lxtheme::StateDot(lxtheme::live(), true); ImGui::SameLine(); }
			ImGui::PushStyleColor(ImGuiCol_Text, is_active ? lxtheme::live() : (is_sel ? lxtheme::text() : lxtheme::text2()));
			if (ImGui::Selectable(prog->mName.c_str(), is_sel))
				mCuedProgram = prog.get();
			ImGui::PopStyleColor();
			ImGui::PopID();
		}

		ImGui::Separator();
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputText("##newprog", mNewProgramName, sizeof(mNewProgramName));
		if (lxagent::Button("+ New program", ImVec2(-1.0f, 0.0f)) && std::strlen(mNewProgramName) > 0)
		{
			lx::Program* np = mLxControlService->createProgram(mNewProgramName);
			if (np != nullptr) mCuedProgram = np;
			mNewProgramName[0] = '\0';
		}
	}


	void lxcontrolApp::drawRoutingPanel(lx::Program* prog)
	{
		if (prog == nullptr)
		{
			ImGui::Spacing();
			ImGui::TextDisabled("No program selected.");
			ImGui::TextDisabled("Pick one on the left, or + New program.");
			ImGui::Spacing();
			ImGui::TextDisabled("A Program is a loadable set of controls playing patches on your fixtures:");
			ImGui::TextDisabled("  1 build a Patch   2 add a Control   3 Route control -> patch   4 Load + play");
			return;
		}

		lx::Program* active = mLxControlService->getActiveProgram();
		const bool is_loaded = (prog == active);
		const auto& triggers = mLxControlService->getTriggers();
		const auto& controls = mLxControlService->getControls();

		// --- Header: name + loaded/Load + delete ---
		ImGui::TextColored(is_loaded ? lxtheme::live() : lxtheme::text(), "%s", prog->mName.c_str());
		ImGui::SameLine();
		if (is_loaded)
			lxtheme::Chip("- Loaded", lxtheme::live2());
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Text, lxtheme::accent2());
			if (lxagent::SmallButton("Load >")) mLxControlService->loadProgram(prog);
			ImGui::PopStyleColor();
		}
		ImGui::SameLine();
		if (lxtheme::DangerButton("Del"))
			ImGui::OpenPopup("del_prog");
		if (ImGui::BeginPopup("del_prog"))
		{
			ImGui::Text("Delete program \"%s\"?", prog->mName.c_str());
			if (lxtheme::DangerButton("Delete"))
			{
				if (mCuedProgram == prog) mCuedProgram = nullptr;
				mLxControlService->removeProgram(prog);
				ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
				return;	// prog is gone; bail this frame
			}
			ImGui::SameLine();
			if (lxagent::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Editing-vs-live band.
		if (!is_loaded)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, lxtheme::accent2());
			ImGui::TextUnformatted("Editing - not the live program");
			ImGui::PopStyleColor();
		}

		// Control-kind triggers only (Enter/Exit auto-fire on load/unload).
		std::vector<lx::Trigger*> control_triggers;
		for (auto& t : triggers)
			if (t->mKind == lx::ETriggerKind::Control) control_triggers.emplace_back(t.get());

		// Controls already mapped to any Trigger in this Program (one mapping per Control, enforced by service).
		std::vector<lx::Control*> mapped_in_program;
		for (auto& m : prog->mControlMappings)
			if (m->mControl != nullptr)
				mapped_in_program.emplace_back(m->mControl.get());

		// --- ROUTING matrix: one flat block per control-trigger (control -> patch -> fixtures + test) ---
		lxtheme::SectionHeader("Routing");
		for (auto* t : control_triggers)
		{
			ImGui::PushID(t->mID.c_str());
			bool active_t = mLxControlService->isTriggerActive(*t);

			// Row head: trigger name (+ active dot), Test fire/stop, Delete.
			if (active_t) { lxtheme::StateDot(lxtheme::live(), true); ImGui::SameLine(); }
			ImGui::TextColored(active_t ? lxtheme::live2() : lxtheme::text(), "%s", t->mName.c_str());
			ImGui::SameLine(); if (lxagent::SmallButton("Fire")) mLxControlService->fireTrigger(*t);
			ImGui::SameLine(); if (lxagent::SmallButton("Stop")) mLxControlService->stopTrigger(*t);
			ImGui::SameLine();
			if (lxagent::SmallButton("Del##trig")) { mLxControlService->removeTrigger(t); ImGui::PopID(); break; }

			// Patch -> fixtures summary for this trigger (from its bindings).
			for (auto& b : t->mBindings)
			{
				ImGui::TextColored(lxtheme::mod2(), "  -> %s", b.mPatch != nullptr ? b.mPatch->mName.c_str() : "(no patch)");
				ImGui::SameLine();
				std::string fx = "[" + std::to_string(b.mFixtureNames.size()) + " fx]";
				lxtheme::Chip(fx.c_str());
			}
			if (t->mBindings.empty())
			{
				ImGui::PushStyleColor(ImGuiCol_Text, lxtheme::live2());
				ImGui::TextUnformatted("  ! no patch bound");
				ImGui::PopStyleColor();
			}

			// Which controls fire this trigger (retarget combo + remove), + add-binding.
			if (!controls.empty())
			{
				std::vector<lx::ControlMapping*> rows;
				for (auto& m : prog->mControlMappings)
					if (m->mTrigger.get() == t) rows.emplace_back(m.get());

				bool mutated = false;
				for (auto* row : rows)
				{
					ImGui::PushID(row->mID.c_str());
					lx::Control* cur = row->mControl.get();
					std::vector<lx::Control*> avail; std::vector<const char*> labels; int cur_idx = 0;
					for (auto& c : controls)
					{
						bool taken = std::find(mapped_in_program.begin(), mapped_in_program.end(), c.get()) != mapped_in_program.end();
						if (taken && c.get() != cur) continue;
						if (c.get() == cur) cur_idx = static_cast<int>(avail.size());
						avail.emplace_back(c.get()); labels.emplace_back(c->mName.c_str());
					}
					ImGui::TextUnformatted("  ctrl"); ImGui::SameLine(); ImGui::SetNextItemWidth(150);
					if (ImGui::Combo("##ctrl", &cur_idx, labels.data(), static_cast<int>(labels.size())))
					{
						lx::Control* picked = avail[cur_idx];
						if (picked != cur && cur != nullptr)
						{
							mLxControlService->clearControlMapping(*prog, *cur);
							mLxControlService->setControlMapping(*prog, *picked, t);
							mutated = true;
						}
					}
					ImGui::SameLine();
					if (lxagent::SmallButton("x") && cur != nullptr) { mLxControlService->clearControlMapping(*prog, *cur); mutated = true; }
					ImGui::PopID();
					if (mutated) break;
				}
				if (!mutated)
				{
					lx::Control* next_avail = nullptr;
					for (auto& c : controls)
						if (std::find(mapped_in_program.begin(), mapped_in_program.end(), c.get()) == mapped_in_program.end()) { next_avail = c.get(); break; }
					bool can_add = (next_avail != nullptr);
					bool dis = lxtheme::PushDisabled(!can_add);
					if (lxagent::SmallButton("+ route a control") && can_add)
						mLxControlService->setControlMapping(*prog, *next_avail, t);
					lxtheme::PopDisabled(dis);
					if (!can_add && ImGui::IsItemHovered())
						ImGui::SetTooltip("Every control is already routed in this program");
				}
			}
			else
			{
				ImGui::TextDisabled("  create a Control first (CONTROLS tab)");
			}

			// Detailed binding editor on demand (kept from the working authoring path).
			drawTriggerBindingsEditor(*t);
			ImGui::Separator();
			ImGui::PopID();
		}
		drawTriggerCreationForm(*prog);

		// --- AUTOMATIC: Enter/Exit lifecycle triggers ---
		lxtheme::SectionHeader("Automatic");
		for (auto& t : triggers)
		{
			if (t->mKind != lx::ETriggerKind::Enter && t->mKind != lx::ETriggerKind::Exit) continue;
			ImGui::PushID(t->mID.c_str());
			bool member = false;
			for (auto& pt : prog->mLifecycleTriggers)
				if (pt.get() == t.get()) { member = true; break; }
			if (ImGui::Checkbox(t->mName.c_str(), &member))
			{
				auto list = prog->mLifecycleTriggers;
				if (member) list.emplace_back(nap::ResourcePtr<lx::Trigger>(t.get()));
				else list.erase(std::remove_if(list.begin(), list.end(),
					[&t](const nap::ResourcePtr<lx::Trigger>& x) { return x.get() == t.get(); }), list.end());
				mLxControlService->setProgramLifecycleTriggers(*prog, list);
			}
			ImGui::SameLine();
			lxtheme::Chip(t->mKind == lx::ETriggerKind::Enter ? "On load" : "On exit");
			ImGui::SameLine(); if (lxagent::SmallButton("Fire")) mLxControlService->fireTrigger(*t.get());
			ImGui::SameLine(); if (lxagent::SmallButton("Stop")) mLxControlService->stopTrigger(*t.get());
			ImGui::SameLine();
			if (lxagent::SmallButton("Del##life")) { mLxControlService->removeTrigger(t.get()); ImGui::PopID(); break; }
			if (member) drawTriggerBindingsEditor(*t.get());
			ImGui::PopID();
		}

		// --- OUTPUT: honest per-fixture summary (dimmer level + claim count) ---
		lxtheme::SectionHeader("Output   held control wins - newest on top");
		for (auto* fx : mLxControlService->getFixturesPhysicalOrder())
		{
			ImGui::PushID(fx);
			size_t claims = 0;
			for (auto* ch : fx->getChannels()) claims += ch->getClaimCount();
			ImGui::PushStyleColor(ImGuiCol_Text, lxtheme::muted());
			ImGui::Text("%s", fx->getDisplayName().c_str());
			ImGui::PopStyleColor();
			ImGui::SameLine(120.0f);
			lxtheme::Fader("##odim", roleOutput(*fx, lx::EChannelRole::Dimmer), ImVec2(100, 12), lxtheme::live());
			ImGui::SameLine();
			if (claims > 0) ImGui::TextColored(lxtheme::live2(), "%d claim%s", static_cast<int>(claims), claims == 1 ? "" : "s");
			else ImGui::TextDisabled("base");
			ImGui::PopID();
		}
	}


	void lxcontrolApp::drawTriggerCreationForm(lx::Program& program)
	{
		static const char* type_labels[] = { "Control", "Enter", "Exit" };

		auto& form = mNewTriggerFormByProgram[program.mID];
		ImGui::InputText("Name##newtrig", form.mName, sizeof(form.mName));
		ImGui::SameLine(); ImGui::SetNextItemWidth(120);
		ImGui::Combo("Type##newtrig", &form.mType, type_labels, 3);
		ImGui::SameLine();
		if (lxagent::Button("+ New Trigger") && std::strlen(form.mName) > 0)
		{
			lx::ETriggerKind kind = form.mType == 1 ? lx::ETriggerKind::Enter
				: form.mType == 2 ? lx::ETriggerKind::Exit
				: lx::ETriggerKind::Control;
			auto* new_trig = mLxControlService->createTrigger(kind, form.mName);
			// Enter/Exit triggers auto-join this Program's lifecycle list (one-step workflow);
			// Control-kind triggers are created unassigned - map them via the Control Mappings menu.
			if (form.mType == 1 || form.mType == 2)
			{
				auto list = program.mLifecycleTriggers;
				list.emplace_back(nap::ResourcePtr<lx::Trigger>(new_trig));
				mLxControlService->setProgramLifecycleTriggers(program, list);
			}
			form.mName[0] = '\0';
		}
	}


	// Desk vocabulary for EControlMode: Hold=Momentary, Latch=Toggle, Trig=FireOnly (findings §92).
	static const char* kModeLabels[] = { "Hold", "Latch", "Trig" };
	static const char* kModeTips[] = {
		"Hold (Momentary): fires while held, releases (falls back) on note-off.",
		"Latch (Toggle): each press toggles the trigger on / off.",
		"Trig (FireOnly): each press re-fires; never holds."
	};

	// Draws one Control row (mode combo + group + Learn/Cancel + Delete-confirm + its bindings).
	// Returns true if the control was deleted (caller must stop iterating that vector).
	bool lxcontrolApp::drawControlRow(lx::Control* c)
	{
		ImGui::PushID(c);

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(c->mName.c_str());

		// Mode: Hold / Latch / Trig, with an explanatory tooltip.
		int mode = static_cast<int>(c->mMode);
		ImGui::SameLine(); ImGui::SetNextItemWidth(80);
		if (ImGui::Combo("##mode", &mode, kModeLabels, 3))
		{
			c->mMode = static_cast<lx::EControlMode>(mode);
			mLxControlService->markDirty();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", kModeTips[mode]);

		// Inline binding readout (mockup "MIDI · Note 36" / "⚠ Not bound"). Device is the card header now.
		int binding_count = 0;
		std::string first_nums;
		for (auto& b : mLxControlService->getBindings())
		{
			if (b->mControl.get() != c) continue;
			if (binding_count == 0)
				for (int n : b->mNumbers) { first_nums += std::to_string(n); first_nums += " "; }
			binding_count++;
		}
		ImGui::SameLine();
		if (binding_count == 0)
			ImGui::TextColored(lxtheme::live2(), "! not bound - Learn");
		else
			ImGui::TextColored(lxtheme::text2(), "MIDI %s", first_nums.empty() ? "(any)" : first_nums.c_str());

		// Learn, with an explicit Cancel (Esc also cancels - see inputMessageReceived).
		ImGui::SameLine();
		if (mLearningControl == c)
		{
			ImGui::TextColored(lxtheme::pulse(), "learning...");
			ImGui::SameLine();
			if (lxagent::SmallButton("Cancel"))
				mLearningControl = nullptr;
			else if (mLxControlService->getMidiEventCounter() > mLearnStartCounter)
			{
				MidiEvent ev = mLxControlService->getLastMidiEvent();
				mLxControlService->createBinding(ev, *c);
				mLearningControl = nullptr;
			}
		}
		else if (lxagent::SmallButton("Learn"))
		{
			mLearningControl = c;
			mLearnStartCounter = mLxControlService->getMidiEventCounter();
		}

		// Delete, with a confirm popup (destructive - review's Del-confirm).
		ImGui::SameLine();
		if (lxtheme::DangerButton("Del"))
			ImGui::OpenPopup("confirm_del");
		bool deleted = false;
		if (ImGui::BeginPopup("confirm_del"))
		{
			ImGui::Text("Delete control \"%s\"?", c->mName.c_str());
			if (lxtheme::DangerButton("Delete"))
			{
				mLxControlService->removeControl(c);
				deleted = true;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (lxagent::Button("Cancel"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// Bindings (learned MIDI numbers) with per-binding remove.
		if (!deleted)
		{
			for (auto& b : mLxControlService->getBindings())
			{
				if (b->mControl.get() != c) continue;
				ImGui::PushID(b.get());
				std::string nums;
				for (int n : b->mNumbers) { nums += std::to_string(n); nums += " "; }
				ImGui::BulletText("MIDI num: %s", nums.empty() ? "(any)" : nums.c_str());
				ImGui::SameLine();
				if (lxagent::SmallButton("X")) mLxControlService->removeBinding(b.get());
				ImGui::PopID();
			}
		}

		ImGui::PopID();
		return deleted;
	}

	void lxcontrolApp::drawControlsTab()
	{
		const auto& controls = mLxControlService->getControls();

		// Two columns: controllers (device cards) | devices + incoming monitor.
		ImGui::BeginChild("ctrl_left", ImVec2(ImGui::GetContentRegionAvail().x - 336.0f, 0), false);

		lxtheme::SectionHeader("Controllers");

		// New control form.
		ImGui::SetNextItemWidth(140);
		ImGui::InputText("Name##ctrl", mNewControlName, sizeof(mNewControlName));
		ImGui::SameLine(); ImGui::SetNextItemWidth(90);
		ImGui::Combo("Mode##ctrl", &mNewControlMode, kModeLabels, 3);
		ImGui::SameLine(); ImGui::SetNextItemWidth(120);
		ImGui::InputText("Device##ctrl", mNewControlGroup, sizeof(mNewControlGroup));
		ImGui::SameLine();
		if (lxagent::Button("+ Add control") && std::strlen(mNewControlName) > 0)
		{
			lx::Control* nc = mLxControlService->createControl(mNewControlName, static_cast<lx::EControlMode>(mNewControlMode));
			if (nc != nullptr) { nc->mGroup = mNewControlGroup; mLxControlService->markDirty(); }
			mNewControlName[0] = '\0';
		}

		if (controls.empty())
		{
			ImGui::Spacing();
			ImGui::TextDisabled("No controls yet. Add one above, then Learn a MIDI message to bind it.");
			ImGui::EndChild();
		}
		else
		{
			// Distinct device groups in first-seen order; ungrouped drawn last.
			std::vector<std::string> groups;
			bool has_ungrouped = false;
			for (auto& c : controls)
			{
				if (c->mGroup.empty()) { has_ungrouped = true; continue; }
				if (std::find(groups.begin(), groups.end(), c->mGroup) == groups.end())
					groups.emplace_back(c->mGroup);
			}
			if (has_ungrouped) groups.emplace_back(std::string());

			const double since = mLastMidiSeen < 0.0 ? -1.0 : ImGui::GetTime() - mLastMidiSeen;
			for (const auto& g : groups)
			{
				ImGui::PushID(g.c_str());
				// Device card header (Controller chip + name + activity), then its control rows. Drawn
				// flat (not a nested child) so every card reliably shows and auto-sizes to its rows.
				ImGui::Spacing();
				lxtheme::Chip("Controller");
				ImGui::SameLine();
				ImGui::TextColored(lxtheme::text(), "%s", g.empty() ? "(ungrouped)" : g.c_str());
				ImGui::SameLine();
				if (since < 0.0) ImGui::TextDisabled("   no messages yet");
				else ImGui::TextDisabled("   last msg %.1fs", since);
				ImGui::Separator();

				bool deleted = false;
				for (auto& c : controls)
				{
					if (c->mGroup != g) continue;
					if (drawControlRow(c.get())) { deleted = true; break; }
				}
				ImGui::PopID();
				if (deleted) { ImGui::EndChild(); return; }	// controls mutated
			}
			ImGui::EndChild();
		}

		// Right column: devices + incoming monitor.
		ImGui::SameLine();
		ImGui::BeginChild("ctrl_right", ImVec2(320, 0), false);
		lxtheme::SectionHeader("Devices");
		std::string port_names = mMidiPort->getPortNames();
		lxtheme::Chip(port_names.empty() ? "(none) - startup snapshot" : port_names.c_str());
		ImGui::PushStyleColor(ImGuiCol_Text, lxtheme::muted());
		ImGui::PushTextWrapPos(0.0f);	// wrap at the child's right edge so sentences don't clip
		ImGui::TextWrapped("napmidi has no hot-plug enumeration - this is the startup snapshot.");
		ImGui::TextWrapped("Silence > 30s may mean a device was unplugged.");
		ImGui::PopTextWrapPos();
		ImGui::PopStyleColor();

		lxtheme::SectionHeader("Incoming");
		ImGui::BeginChild("MidiLog", ImVec2(0, 160), true);
		for (const auto& line : mLxControlService->getMidiLog())
			ImGui::TextUnformatted(line.c_str());
		ImGui::EndChild();
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
			{
				if (mLearningControl != nullptr)
					mLearningControl = nullptr;	// Esc cancels an in-progress Learn instead of quitting
				else
					quit();
			}
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
