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
				// The patch editor's left edge is a draggable splitter (mPatchPanelWidth).
				lx::Program* cued = mCuedProgram != nullptr ? mCuedProgram : mLxControlService->getActiveProgram();
				const float sp = ImGui::GetStyle().ItemSpacing.x;
				const float total = ImGui::GetContentRegionAvail().x;
				const float rowH = ImGui::GetContentRegionAvail().y;
				const float listW = 190.0f;
				const float splitW = 6.0f;
				mPatchPanelWidth = nap::math::clamp(mPatchPanelWidth, 320.0f, std::max(360.0f, total - listW - 320.0f));
				float routeW = total - listW - mPatchPanelWidth - splitW - 3.0f * sp;
				if (routeW < 200.0f) routeW = 200.0f;

				ImGui::BeginChild("prog_list", ImVec2(listW, rowH), true);
				drawProgramsListPanel();
				ImGui::EndChild();
				ImGui::SameLine();
				ImGui::BeginChild("prog_route", ImVec2(routeW, rowH), true);
				drawRoutingPanel(cued);
				ImGui::EndChild();

				// Draggable splitter (no imgui_internal): an InvisibleButton grip that adjusts the width.
				ImGui::SameLine();
				ImGui::InvisibleButton("##patch_splitter", ImVec2(splitW, rowH));
				if (ImGui::IsItemHovered() || ImGui::IsItemActive())
					ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
				if (ImGui::IsItemActive())
					mPatchPanelWidth -= ImGui::GetIO().MouseDelta.x;	// drag left = wider patch editor
				ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
					ImGui::ColorConvertFloat4ToU32((ImGui::IsItemActive() || ImGui::IsItemHovered()) ? lxtheme::accent() : lxtheme::border()));

				ImGui::SameLine();
				ImGui::BeginChild("prog_patch", ImVec2(mPatchPanelWidth, rowH), true);
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


	// Human label for a source parameter (its role / kind), so combos + "drives X" read meaningfully
	// instead of the default "Param". Float/Toggle -> role name; Color -> "Color".
	static const char* kRoleLabels[] = { "Dimmer", "Strobe", "Red", "Green", "Blue", "ColorMacro", "SoundMode", "Generic" };
	static std::string patchParamLabel(lx::PatchParameter* p)
	{
		if (p == nullptr) return "(none)";
		if (auto* fp = rtti_cast<lx::FloatParameter>(p)) return kRoleLabels[nap::math::clamp(static_cast<int>(fp->mRole), 0, 7)];
		if (rtti_cast<lx::ColorParameter>(p)) return "Color";
		if (auto* tp = rtti_cast<lx::ToggleParameter>(p)) return std::string(kRoleLabels[nap::math::clamp(static_cast<int>(tp->mRole), 0, 7)]) + " (tgl)";
		return p->mName;
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

				// Live "current" readout of this patch's sources (post-modulation values, updated each
				// frame by Patch::update). Horizontal meters (value = fill width, with a % readout) so the
				// bar's long axis actually encodes the value; one meter/swatch per fixture voice in
				// Multiple-spread mode (a single bar can't represent per-fixture values).
				if (!patch->mParameters.empty())
				{
					lxtheme::SectionHeader("Current");
					const int voices = (patch->mTargetMode == lx::EPatchTargetMode::Multiple)
						? nap::math::clamp(patch->mFixtureCount, 1, 32) : 1;
					for (auto& p : patch->mParameters)
					{
						ImGui::PushID(p.get());
						ImGui::AlignTextToFramePadding();
						ImGui::PushStyleColor(ImGuiCol_Text, lxtheme::muted());
						ImGui::TextUnformatted(patchParamLabel(p.get()).c_str());
						ImGui::PopStyleColor();
						ImGui::SameLine(150.0f);

						if (rtti_cast<lx::ColorParameter>(p.get()))
						{
							for (int s = 0; s < voices; ++s)
							{
								ImGui::PushID(s);
								float rgb[3] = { p->getComponentValue(s, 0), p->getComponentValue(s, 1), p->getComponentValue(s, 2) };
								lxtheme::Swatch("##cur", rgb, 18.0f);
								if (voices > 1 && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", describePatchVoice(patch.get(), s).c_str());
								ImGui::PopID();
								if (s + 1 < voices) ImGui::SameLine();
							}
						}
						else
						{
							ImGui::PushStyleColor(ImGuiCol_PlotHistogram, lxtheme::live());
							if (voices == 1)
							{
								ImGui::ProgressBar(p->getComponentValue(0, 0), ImVec2(-1.0f, 14.0f));
							}
							else
							{
								for (int s = 0; s < voices; ++s)
								{
									ImGui::PushID(s);
									float v = p->getComponentValue(s, 0);
									ImGui::ProgressBar(v, ImVec2(70.0f, 14.0f));
									if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s = %.2f", describePatchVoice(patch.get(), s).c_str(), v);
									ImGui::PopID();
									if (s + 1 < voices) ImGui::SameLine();
								}
							}
							ImGui::PopStyleColor();
						}
						ImGui::PopID();
					}
				}

				// --- SOURCE zone: editable parameters, one per row (role + base + Del) ---
				lxtheme::SectionHeader("Source");
				bool src_removed = false;
				for (auto& p : patch->mParameters)
				{
					ImGui::PushID(p.get());
					if (auto* fp = rtti_cast<lx::FloatParameter>(p.get()))
					{
						int role = static_cast<int>(fp->mRole);
						ImGui::SetNextItemWidth(120);
						if (ImGui::Combo("##role", &role, role_labels, 8)) fp->mRole = static_cast<lx::EChannelRole>(role);
						ImGui::SameLine(); ImGui::SetNextItemWidth(-52.0f);
						ImGui::SliderFloat("##base", &fp->mValue, 0.0f, 1.0f);
					}
					else if (auto* cp = rtti_cast<lx::ColorParameter>(p.get()))
					{
						float col[3] = { cp->mRed, cp->mGreen, cp->mBlue };
						ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Color"); ImGui::SameLine();
						if (ImGui::ColorEdit3("##color", col, ImGuiColorEditFlags_NoInputs))
						{ cp->mRed = col[0]; cp->mGreen = col[1]; cp->mBlue = col[2]; }
						ImGui::SameLine();
						ImGui::TextDisabled("%d . %d . %d", static_cast<int>(col[0] * 255), static_cast<int>(col[1] * 255), static_cast<int>(col[2] * 255));
					}
					else if (auto* tp = rtti_cast<lx::ToggleParameter>(p.get()))
					{
						int role = static_cast<int>(tp->mRole);
						ImGui::SetNextItemWidth(120);
						if (ImGui::Combo("##trole", &role, role_labels, 8)) tp->mRole = static_cast<lx::EChannelRole>(role);
						ImGui::SameLine(); lxtheme::LabeledCheck("On", &tp->mValue);
					}
					ImGui::SameLine();
					if (lxtheme::DangerButton("Del")) { mLxControlService->removePatchParameter(*patch.get(), p.get()); src_removed = true; }
					ImGui::PopID();
					if (src_removed) break;	// mParameters mutated
				}
				if (lxagent::Button("+ Float")) mLxControlService->addPatchParameter(*patch.get(), RTTI_OF(lx::FloatParameter));
				ImGui::SameLine(); if (lxagent::Button("+ Color")) mLxControlService->addPatchParameter(*patch.get(), RTTI_OF(lx::ColorParameter));
				ImGui::SameLine(); if (lxagent::Button("+ Toggle")) mLxControlService->addPatchParameter(*patch.get(), RTTI_OF(lx::ToggleParameter));

				// --- MODULATION zone: add via a type dropdown + Add button ---
				lxtheme::SectionHeader("Modulation");
				static const char* mod_type_labels[] = { "ADSR", "AD", "LFO", "Step", "Chase", "Noise" };
				const nap::rtti::TypeInfo mod_types[] = {
					RTTI_OF(lx::AdsrModulator), RTTI_OF(lx::AdModulator), RTTI_OF(lx::LfoModulator),
					RTTI_OF(lx::StepModulator), RTTI_OF(lx::ChaseModulator), RTTI_OF(lx::NoiseModulator) };
				ImGui::SetNextItemWidth(130);
				ImGui::Combo("##addmodtype", &mAddModType, mod_type_labels, 6);
				ImGui::SameLine();
				const bool can_add_mod = !patch->mParameters.empty();
				bool addmod_dis = lxtheme::PushDisabled(!can_add_mod);
				if (lxagent::Button("+ Add modulator") && can_add_mod)
					mLxControlService->addModulator(*patch.get(), mod_types[nap::math::clamp(mAddModType, 0, 5)], patch->mParameters[0].get());
				lxtheme::PopDisabled(addmod_dis);
				if (!can_add_mod && ImGui::IsItemHovered())
					ImGui::SetTooltip("Add a source parameter first");

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
					ImGui::SameLine(); if (lxagent::SmallButton("Trigger")) m->onTrigger();
					ImGui::SameLine(); if (lxagent::SmallButton("Stop")) m->onStop();
					ImGui::SameLine();
					if (lxtheme::DangerButton("Del")) { mLxControlService->removeModulator(*patch.get(), m.get()); ImGui::PopID(); break; }

					// Mod-matrix: this modulator drives every target in mTargets. Chips (+ x to remove),
					// then a combo to add another source parameter as a target.
					ImGui::AlignTextToFramePadding();
					ImGui::TextColored(lxtheme::mod2(), "drives");
					int remove_ti = -1;
					for (size_t ti = 0; ti < m->mTargets.size(); ++ti)
					{
						ImGui::SameLine();
						ImGui::PushID(static_cast<int>(ti));
						lxtheme::Chip(patchParamLabel(m->mTargets[ti].get()).c_str(), lxtheme::mod2());
						ImGui::SameLine(0.0f, 2.0f);
						if (lxagent::SmallButton("x")) remove_ti = static_cast<int>(ti);
						ImGui::PopID();
					}
					if (remove_ti >= 0) { m->mTargets.erase(m->mTargets.begin() + remove_ti); mLxControlService->markDirty(); }
					{
						// Params not yet targeted -> a "+ add" combo.
						std::vector<lx::PatchParameter*> avail;
						std::vector<std::string> avail_lbls;
						for (auto& p : patch->mParameters)
						{
							bool already = false;
							for (auto& t : m->mTargets) if (t.get() == p.get()) { already = true; break; }
							if (!already) { avail.emplace_back(p.get()); avail_lbls.emplace_back(patchParamLabel(p.get())); }
						}
						if (!avail.empty())
						{
							std::vector<const char*> items; items.emplace_back("+ add");
							for (auto& s : avail_lbls) items.emplace_back(s.c_str());
							int sel = 0;
							ImGui::SameLine(); ImGui::SetNextItemWidth(110);
							if (ImGui::Combo("##addtarget", &sel, items.data(), static_cast<int>(items.size())) && sel > 0)
							{
								m->mTargets.emplace_back(avail[sel - 1]);
								mLxControlService->markDirty();
							}
						}
					}

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
						// Static shape + playhead at the real transport phase (the mockup's modulator preview).
						float shape[128];
						for (int i = 0; i < 128; ++i) shape[i] = m->sampleShape(i / 127.0f);
						lxtheme::PlayheadPreview(shape, 128, m->playheadPhase(), ImVec2(-1.0f, 40.0f));
					}

					auto regen = [&]() { m->generateCurve(*mLxControlService); };

					if (auto* adsr = rtti_cast<lx::AdsrModulator>(m.get()))
					{
						bool ch = false;
						ch |= lxtheme::LabeledDrag("A", &adsr->mAttack, 0.01f, 0.0f, 10.0f); ImGui::SameLine();
						ch |= lxtheme::LabeledDrag("D", &adsr->mDecay, 0.01f, 0.0f, 10.0f); ImGui::SameLine();
						ch |= lxtheme::LabeledDrag("S", &adsr->mSustain, 0.01f, 0.0f, 1.0f); ImGui::SameLine();
						ch |= lxtheme::LabeledDrag("R", &adsr->mRelease, 0.01f, 0.0f, 10.0f); ImGui::SameLine();
						ch |= lxtheme::LabeledCheck("Loop", &adsr->mLoop);
						if (ch) regen();
					}
					else if (auto* ad = rtti_cast<lx::AdModulator>(m.get()))
					{
						bool ch = false;
						ch |= lxtheme::LabeledDrag("A", &ad->mAttack, 0.01f, 0.0f, 10.0f); ImGui::SameLine();
						ch |= lxtheme::LabeledDrag("D", &ad->mDecay, 0.01f, 0.0f, 10.0f); ImGui::SameLine();
						int amode = static_cast<int>(ad->mMode);
						if (lxtheme::LabeledCombo("Mode", &amode, ad_mode_labels, 2, 150)) ad->mMode = static_cast<lx::EAdMode>(amode);
						if (ch) regen();
					}
					else if (auto* lfo = rtti_cast<lx::LfoModulator>(m.get()))
					{
						int shape = static_cast<int>(lfo->mShape);
						if (lxtheme::LabeledCombo("Shape", &shape, shape_labels, 6, 110)) { lfo->mShape = static_cast<lx::ELfoShape>(shape); regen(); }
						ImGui::SameLine();
						if (lxtheme::LabeledDrag("Hz", &lfo->mFrequency, 0.05f, 0.0f, 30.0f, 80.0f) && lfo->mPlayer != nullptr)
							lfo->mPlayer->setPlaybackSpeed(lfo->mFrequency);
						ImGui::SameLine();
						int lmode = static_cast<int>(lfo->mMode);
						if (lxtheme::LabeledCombo("Mode", &lmode, lfo_mode_labels, 3, 120)) lfo->mMode = static_cast<lx::ELfoMode>(lmode);
					}
					else if (auto* step = rtti_cast<lx::StepModulator>(m.get()))
					{
						bool ch = false;
						ch |= lxtheme::LabeledDrag("Rate", &step->mRate, 0.1f, 0.1f, 30.0f, 80.0f); ImGui::SameLine();
						ch |= lxtheme::LabeledCheck("Glide", &step->mGlide);
						if (ch) regen();
					}
					else if (auto* chase = rtti_cast<lx::ChaseModulator>(m.get()))
					{
						lxtheme::LabeledDrag("Rate", &chase->mRate, 0.05f, 0.0f, 30.0f, 80.0f); ImGui::SameLine();
						lxtheme::LabeledSlider("PulseWidth", &chase->mPulseWidth, 0.01f, 1.0f, 80.0f); ImGui::SameLine();
						lxtheme::LabeledCheck("Glide", &chase->mGlide);
					}
					else if (auto* noise = rtti_cast<lx::NoiseModulator>(m.get()))
					{
						lxtheme::LabeledDrag("Rate", &noise->mRate, 0.05f, 0.0f, 30.0f, 80.0f); ImGui::SameLine();
						lxtheme::LabeledSlider("Smoothing", &noise->mSmoothing, 0.0f, 1.0f, 100.0f); ImGui::SameLine();
						lxtheme::LabeledInt("Seed", &noise->mSeed, 80.0f);
					}

					int blend = static_cast<int>(m->mBlend);
					if (lxtheme::LabeledCombo("Blend", &blend, blend_labels, 3, 100)) m->mBlend = static_cast<lx::EModulatorBlend>(blend);
					ImGui::SameLine(); lxtheme::LabeledDrag("Min", &m->mMin, 0.01f, 0.0f, 1.0f, 70.0f);
					ImGui::SameLine(); lxtheme::LabeledDrag("Max", &m->mMax, 0.01f, 0.0f, 1.0f, 70.0f);
					ImGui::PopID();
				}
			}
			ImGui::PopID();
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

		// Controls already routed in this program (one routing per control).
		std::vector<lx::Control*> mapped_in_program;
		for (auto& m : prog->mControlMappings)
			if (m->mControl != nullptr)
				mapped_in_program.emplace_back(m->mControl.get());

		const auto& patches = mLxControlService->getPatches();
		auto fixtures = mLxControlService->getFixturesPhysicalOrder();

		auto controlBound = [&](lx::Control* c) -> bool {
			for (auto& b : mLxControlService->getBindings())
				if (b->mControl.get() == c) return true;
			return false;
		};

		// Toggle chips for a trigger's single-binding fixtures (S1/S2/...), edited via setRoutingFixtures.
		auto fixtureChips = [&](lx::Trigger* trig) {
			static const std::vector<std::string> kEmpty;
			const std::vector<std::string>& sel = trig->mBindings.empty() ? kEmpty : trig->mBindings[0].mFixtureNames;
			for (int i = 0; i < static_cast<int>(fixtures.size()); ++i)
			{
				const std::string eid = fixtures[i]->getEntityID();
				const bool in = std::find(sel.begin(), sel.end(), eid) != sel.end();
				ImGui::PushID(i);
				ImGui::PushStyleColor(ImGuiCol_Button, in ? lxtheme::rgb(0x2dd4bf, 0.35f) : lxtheme::bar());
				ImGui::PushStyleColor(ImGuiCol_Text, in ? lxtheme::text() : lxtheme::muted());
				if (lxagent::SmallButton((std::string("S") + std::to_string(i + 1)).c_str()))
				{
					std::vector<std::string> next(sel.begin(), sel.end());
					if (in) next.erase(std::remove(next.begin(), next.end(), eid), next.end());
					else next.emplace_back(eid);
					mLxControlService->setRoutingFixtures(*trig, next);
				}
				ImGui::PopStyleColor(2);
				ImGui::PopID();
				ImGui::SameLine();
			}
		};

		// --- ROUTING: one control-first row per mapping (Control -> Patch -> fixtures + test) ---
		lxtheme::SectionHeader("Routing");
		lx::ControlMapping* unroute_target = nullptr;
		for (auto& m : prog->mControlMappings)
		{
			lx::Control* ctrl = m->mControl.get();
			lx::Trigger* trig = m->mTrigger.get();
			if (ctrl == nullptr || trig == nullptr) continue;
			ImGui::PushID(m->mID.c_str());

			if (mLxControlService->isTriggerActive(*trig)) { lxtheme::StateDot(lxtheme::live(), true); ImGui::SameLine(); }

			// [Control v]: this row's control + any control not routed elsewhere in the program.
			std::vector<lx::Control*> cavail; std::vector<const char*> clabels; int cidx = 0;
			for (auto& c : controls)
			{
				const bool taken = std::find(mapped_in_program.begin(), mapped_in_program.end(), c.get()) != mapped_in_program.end();
				if (taken && c.get() != ctrl) continue;
				if (c.get() == ctrl) cidx = static_cast<int>(cavail.size());
				cavail.emplace_back(c.get()); clabels.emplace_back(c->mName.c_str());
			}
			ImGui::SetNextItemWidth(110);
			if (ImGui::Combo("##ctrl", &cidx, clabels.data(), static_cast<int>(clabels.size())) && cavail[cidx] != ctrl)
			{
				// Retarget which control fires this routing. This mutates mControlMappings, so bail the
				// loop right after (don't touch the now-stale m/trig this frame).
				mLxControlService->clearControlMapping(*prog, *ctrl);
				mLxControlService->setControlMapping(*prog, *cavail[cidx], trig);
				ImGui::PopID();
				break;
			}

			// Unbound-control warning.
			if (!controlBound(ctrl))
			{
				ImGui::SameLine();
				ImGui::TextColored(lxtheme::live2(), "!unbound");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("This control has no MIDI binding - Learn one in CONTROLS");
			}

			ImGui::SameLine(); ImGui::TextColored(lxtheme::muted(), "->");

			// [Patch v] (edits the routing's single binding).
			lx::Patch* curp = trig->mBindings.empty() ? nullptr : trig->mBindings[0].mPatch.get();
			std::vector<const char*> plabels; plabels.emplace_back("(no patch)"); int pidx = 0;
			for (int i = 0; i < static_cast<int>(patches.size()); ++i)
			{
				plabels.emplace_back(patches[i]->mName.c_str());
				if (patches[i].get() == curp) pidx = i + 1;
			}
			ImGui::SameLine(); ImGui::SetNextItemWidth(120);
			if (ImGui::Combo("##patch", &pidx, plabels.data(), static_cast<int>(plabels.size())))
				mLxControlService->setRoutingPatch(*trig, pidx == 0 ? nullptr : patches[pidx - 1].get());

			// Fixture chips + test + remove.
			ImGui::SameLine();
			fixtureChips(trig);
			if (lxagent::SmallButton("Fire")) mLxControlService->fireTrigger(*trig);
			ImGui::SameLine(); if (lxagent::SmallButton("Stop")) mLxControlService->stopTrigger(*trig);
			ImGui::SameLine(); if (lxtheme::DangerButton("x")) unroute_target = m.get();

			ImGui::PopID();
			ImGui::Separator();
		}
		if (unroute_target != nullptr) mLxControlService->unroute(*prog, unroute_target);

		// + route a control -> creates the (hidden) trigger + binding implicitly.
		{
			lx::Control* next_avail = nullptr;
			for (auto& c : controls)
				if (std::find(mapped_in_program.begin(), mapped_in_program.end(), c.get()) == mapped_in_program.end()) { next_avail = c.get(); break; }
			const bool can_add = (next_avail != nullptr) && !patches.empty();
			const bool dis = lxtheme::PushDisabled(!can_add);
			if (lxagent::Button("+ route a control") && can_add)
				mLxControlService->routeControl(*prog, *next_avail, patches[0].get(), {});
			lxtheme::PopDisabled(dis);
			if (!can_add && ImGui::IsItemHovered())
				ImGui::SetTooltip(controls.empty() ? "Create a Control in CONTROLS first"
					: (patches.empty() ? "Create a Patch first" : "Every control is already routed"));
		}

		// --- AUTOMATIC: On load / On exit lifecycle routings (Enter/Exit triggers, hidden) ---
		lxtheme::SectionHeader("Automatic");
		auto allFixtureIDs = [&]() { std::vector<std::string> v; for (auto* f : fixtures) v.emplace_back(f->getEntityID()); return v; };
		auto lifecycleRow = [&](const char* label, lx::ETriggerKind kind)
		{
			ImGui::PushID(label);
			lxtheme::Chip(label);
			ImGui::SameLine(); ImGui::TextColored(lxtheme::muted(), "->");
			lx::Trigger* t = mLxControlService->getLifecycleTrigger(*prog, kind);
			lx::Patch* cur = (t != nullptr && !t->mBindings.empty()) ? t->mBindings[0].mPatch.get() : nullptr;
			std::vector<const char*> items; items.emplace_back("no action"); int sel = 0;
			for (int i = 0; i < static_cast<int>(patches.size()); ++i)
			{
				items.emplace_back(patches[i]->mName.c_str());
				if (patches[i].get() == cur) sel = i + 1;
			}
			ImGui::SameLine(); ImGui::SetNextItemWidth(130);
			if (ImGui::Combo("##lc", &sel, items.data(), static_cast<int>(items.size())))
			{
				if (sel == 0)
					mLxControlService->clearLifecycle(*prog, kind);
				else if (lx::Trigger* nt = mLxControlService->ensureLifecycleTrigger(*prog, kind))
				{
					const bool wasEmpty = nt->mBindings.empty() || nt->mBindings[0].mFixtureNames.empty();
					mLxControlService->setRoutingPatch(*nt, patches[sel - 1].get());
					if (wasEmpty) mLxControlService->setRoutingFixtures(*nt, allFixtureIDs());	// default: all fixtures
				}
			}
			t = mLxControlService->getLifecycleTrigger(*prog, kind);	// re-fetch (may have just been created/cleared)
			if (t != nullptr && !t->mBindings.empty() && t->mBindings[0].mPatch != nullptr)
			{
				ImGui::SameLine();
				fixtureChips(t);
				if (lxagent::SmallButton("Fire")) mLxControlService->fireTrigger(*t);
				ImGui::SameLine(); if (lxagent::SmallButton("Stop")) mLxControlService->stopTrigger(*t);
			}
			ImGui::PopID();
		};
		lifecycleRow("On load", lx::ETriggerKind::Enter);
		lifecycleRow("On exit", lx::ETriggerKind::Exit);

		// --- OUTPUT: per-fixture claim columns (winner first: held, then newest) ---
		lxtheme::SectionHeader("Output   held control wins - newest on top");
		ImGui::Columns(fixtures.empty() ? 1 : static_cast<int>(fixtures.size()), "##outcols", false);
		for (auto* fx : fixtures)
		{
			ImGui::PushID(fx);
			ImGui::PushStyleColor(ImGuiCol_Text, lxtheme::muted());
			ImGui::TextUnformatted(fx->getDisplayName().c_str());
			ImGui::PopStyleColor();
			auto claimants = mLxControlService->fixtureClaimants(*fx);
			if (claimants.empty())
				ImGui::TextDisabled("base");
			else
				for (size_t k = 0; k < claimants.size(); ++k)
					lxtheme::Chip(claimants[k].c_str(), k == 0 ? lxtheme::live2() : lxtheme::muted());
			ImGui::PopID();
			ImGui::NextColumn();
		}
		ImGui::Columns(1);
		ImGui::TextDisabled("Release everything -> output goes dark.");
	}


	// Desk vocabulary for EControlMode: Hold=Momentary, Latch=Toggle, Trig=FireOnly (findings §92).
	static const char* kModeLabels[] = { "Hold", "Latch", "Trig" };
	static const char* kModeTips[] = {
		"Hold (Momentary): fires while held, releases (falls back) on note-off.",
		"Latch (Toggle): each press toggles the trigger on / off.",
		"Trig (FireOnly): each press re-fires; never holds."
	};

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
