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
#include <cstring>
#include <algorithm>

// lx patch/modulator types (for RTTI_OF dispatch + casts)
#include <channelrole.h>
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

		// Bind subsequent ImGui calls to our single window, then queue our GUI.
		mGuiService->selectWindow(mRenderWindow);
		drawMainUI();
		lxstyleguide::draw(&mShowStyleGuide);	// design-language test bed (own window)
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

		drawLiveBar();
		ImGui::Separator();

		if (mMode == EUiMode::Perform)
		{
			drawPerformGrid();
			ImGui::End();
			return;
		}

		// Edit mode: authoring surfaces. (RIG / PROGRAMS / CONTROLS restructure lands in E2-E4;
		// for now the existing tabs remain, reachable only in Edit mode.)
		if (ImGui::BeginTabBar("MainTabs"))
		{
			if (ImGui::BeginTabItem("Fixtures"))
			{
				drawFixturesTab();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Patches"))
			{
				drawPatchesTab();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Programs"))
			{
				drawProgramsTab();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("CONTROLS"))
			{
				drawControlsTab();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();
	}


	void lxcontrolApp::drawLiveBar()
	{
		lx::Program* active = mLxControlService->getActiveProgram();
		const size_t voices = mLxControlService->activeVoiceCount();

		// Output state dot + label: honest (dark / N held / loaded-idle), never a phantom "on".
		if (voices > 0)
		{
			lxtheme::StateDot(lxtheme::live(), true);
			ImGui::SameLine();
			ImGui::TextColored(lxtheme::live(), "%d voice%s", static_cast<int>(voices), voices == 1 ? "" : "s");
		}
		else
		{
			lxtheme::StateDot(ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
			ImGui::SameLine();
			ImGui::TextDisabled(active != nullptr ? "loaded - idle" : "dark");
		}

		// Loaded program.
		ImGui::SameLine(0.0f, 20.0f);
		if (active != nullptr)
			ImGui::TextColored(lxtheme::accent(), "Program: %s", active->mName.c_str());
		else
			ImGui::TextDisabled("Program: - none -");

		// All Stop (panic).
		ImGui::SameLine(0.0f, 20.0f);
		if (lxtheme::DangerButton("# All Stop"))
			mLxControlService->stopAll();

		// MIDI activity: the last message, not a fake "connected" light (review #3 / findings §6).
		ImGui::SameLine(0.0f, 20.0f);
		if (mLxControlService->hasLastMidiEvent())
		{
			MidiEvent ev = mLxControlService->getLastMidiEvent();
			ImGui::TextDisabled("MIDI: ch%d n%d v%d", ev.getChannel(), ev.getNumber(), ev.getValue());
		}
		else
		{
			ImGui::TextDisabled("MIDI: -");
		}

		// Perform/Edit toggle + Style Guide (right-aligned-ish).
		ImGui::SameLine(0.0f, 24.0f);
		const bool perform = (mMode == EUiMode::Perform);
		if (perform) ImGui::PushStyleColor(ImGuiCol_Button, lxtheme::accent());
		if (ImGui::Button(perform ? "PERFORM" : "EDIT"))
			mMode = perform ? EUiMode::Edit : EUiMode::Perform;
		if (perform) ImGui::PopStyleColor();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Toggle Perform (play-only) / Edit (authoring)");

		ImGui::SameLine();
		ImGui::Checkbox("Style Guide", &mShowStyleGuide);
	}


	void lxcontrolApp::drawPerformGrid()
	{
		lx::Program* active = mLxControlService->getActiveProgram();
		if (active == nullptr)
		{
			ImGui::Spacing();
			ImGui::TextDisabled("No program loaded. Load one in Edit mode to perform it.");
			return;
		}

		const auto& controls = mLxControlService->getControls();
		if (controls.empty())
		{
			ImGui::Spacing();
			ImGui::TextDisabled("No Controls yet. Create some in Edit mode (CONTROLS).");
			return;
		}

		// Play-only pad grid: one pad per Control, firing its mapped Trigger in the active program.
		// Unbound controls are shown dimmed (no trigger to fire), never hidden.
		const float pad = 96.0f;
		const float avail = ImGui::GetContentRegionAvail().x;
		const int per_row = (avail > pad * 1.5f) ? static_cast<int>(avail / (pad + 8.0f)) : 1;
		int col = 0;
		for (auto& c : controls)
		{
			lx::Trigger* trig = mLxControlService->getControlMapping(*active, *c.get());
			const bool bound = (trig != nullptr);
			const bool live = bound && mLxControlService->isTriggerActive(*trig);

			ImGui::PushID(c.get());
			if (live) ImGui::PushStyleColor(ImGuiCol_Button, lxtheme::live());
			const bool dis = lxtheme::PushDisabled(!bound);
			if (ImGui::Button(c->mName.c_str(), ImVec2(pad, pad)) && bound)
				mLxControlService->fireTrigger(*trig);
			lxtheme::PopDisabled(dis);
			if (live) ImGui::PopStyleColor();
			if (!bound && ImGui::IsItemHovered())
				ImGui::SetTooltip("Unbound in this program - map it in Edit / PROGRAMS");
			ImGui::PopID();

			if (++col % per_row != 0)
				ImGui::SameLine();
		}
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

		ImGui::InputText("Name", mNewPatchName, sizeof(mNewPatchName));
		ImGui::SameLine();
		if (ImGui::Button("+ New Patch") && std::strlen(mNewPatchName) > 0)
		{
			mLxControlService->createPatch(mNewPatchName);
			mNewPatchName[0] = '\0';
		}
		ImGui::Separator();

		for (auto& patch : mLxControlService->getPatches())
		{
			ImGui::PushID(patch.get());
			bool open = ImGui::CollapsingHeader(patch->mName.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete"))
			{
				mLxControlService->removePatch(patch.get());
				ImGui::PopID();
				break;
			}

			if (open)
			{
				// --- Target mode: how many independent fixture voices this patch computes. FixtureCount
				// itself is no longer hand-typed here -- it's derived automatically from whichever Trigger
				// binding fires this patch (see lxcontrolService::fireTrigger/syncPatchFixtureCount), so
				// "how many fixtures" can never drift out of sync with what's actually checked in a
				// binding's fixture list. ---
				static const char* target_mode_labels[] = { "Single Fixture", "Multiple Fixtures" };
				int mode = static_cast<int>(patch->mTargetMode);
				ImGui::SetNextItemWidth(160);
				bool mode_changed = ImGui::Combo("Target", &mode, target_mode_labels, 2);
				if (mode == static_cast<int>(lx::EPatchTargetMode::Multiple))
				{
					ImGui::SameLine();
					ImGui::TextDisabled("(%d fixture%s -- set by whichever Trigger binding last fired this patch)",
						patch->mFixtureCount, patch->mFixtureCount == 1 ? "" : "s");
				}
				if (mode_changed)
					mLxControlService->setPatchTargetMode(*patch.get(), static_cast<lx::EPatchTargetMode>(mode));

				ImGui::Separator();

				// --- Parameters ---
				if (ImGui::Button("Add Float"))		mLxControlService->addPatchParameter(*patch.get(), RTTI_OF(lx::FloatParameter));
				ImGui::SameLine();
				if (ImGui::Button("Add Color"))		mLxControlService->addPatchParameter(*patch.get(), RTTI_OF(lx::ColorParameter));
				ImGui::SameLine();
				if (ImGui::Button("Add Toggle"))	mLxControlService->addPatchParameter(*patch.get(), RTTI_OF(lx::ToggleParameter));

				for (auto& p : patch->mParameters)
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
				int& tgt = mModTargetIndex[patch->mID];
				std::vector<const char*> plabels;
				for (auto& p : patch->mParameters) plabels.emplace_back(p->mName.c_str());
				if (!plabels.empty())
				{
					tgt = nap::math::clamp(tgt, 0, static_cast<int>(plabels.size()) - 1);
					ImGui::SetNextItemWidth(140);
					ImGui::Combo("Target", &tgt, plabels.data(), static_cast<int>(plabels.size()));
				}
				auto add_mod = [&](nap::rtti::TypeInfo type)
				{
					if (patch->mParameters.empty()) return;
					int i = nap::math::clamp(tgt, 0, static_cast<int>(patch->mParameters.size()) - 1);
					mLxControlService->addModulator(*patch.get(), type, patch->mParameters[i].get());
				};
				ImGui::SameLine(); if (ImGui::Button("Add ADSR")) add_mod(RTTI_OF(lx::AdsrModulator));
				ImGui::SameLine(); if (ImGui::Button("Add AD"))   add_mod(RTTI_OF(lx::AdModulator));
				ImGui::SameLine(); if (ImGui::Button("Add LFO"))  add_mod(RTTI_OF(lx::LfoModulator));
				ImGui::SameLine(); if (ImGui::Button("Add Step")) add_mod(RTTI_OF(lx::StepModulator));
				ImGui::SameLine(); if (ImGui::Button("Add Chase")) add_mod(RTTI_OF(lx::ChaseModulator));
				ImGui::SameLine(); if (ImGui::Button("Add Noise")) add_mod(RTTI_OF(lx::NoiseModulator));

				static const char* blend_labels[]    = { "Replace", "Multiply", "Add" };
				static const char* lfo_mode_labels[] = { "Loop", "OneShot", "LoopRetrigger" };
				static const char* ad_mode_labels[]  = { "OneShot", "LoopWhileSustained" };

				for (auto& m : patch->mModulators)
				{
					ImGui::PushID(m.get());

					// Chase/Noise drive a distinct value per fixture voice -- a single scalar plot/bar
					// doesn't represent that, so show one mini progress bar per voice instead, labeled with
					// the fixture that voice actually resolves to wherever we can determine it.
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
						// Live shape plot: raw 0..1 sink value over recent frames.
						auto& hist = mModHistory[m->mID];
						hist.push_back(m->mSink != nullptr ? m->mSink->mValue : 0.0f);
						if (hist.size() > 120) hist.erase(hist.begin());	// ponytail: O(n) shift, n=120, negligible
						ImGui::PlotLines("##plot", hist.data(), static_cast<int>(hist.size()), 0, nullptr, 0.0f, 1.0f, ImVec2(160, 40));
						ImGui::ProgressBar(m->value(), ImVec2(120, 0));
					}
					ImGui::SameLine();
					ImGui::Text("-> %s", m->mTarget != nullptr ? m->mTarget->mName.c_str() : "(none)");
					ImGui::SameLine(); if (ImGui::SmallButton("Trigger")) m->onTrigger();
					ImGui::SameLine(); if (ImGui::SmallButton("Stop")) m->onStop();

					// Editing a shape parameter re-authors the curve (main thread -> safe with StandardClock).
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
						int mode = static_cast<int>(ad->mMode);
						ImGui::SetNextItemWidth(150);
						if (ImGui::Combo("Mode##ad", &mode, ad_mode_labels, 2)) ad->mMode = static_cast<lx::EAdMode>(mode);
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
						int mode = static_cast<int>(lfo->mMode);
						ImGui::SameLine(); ImGui::SetNextItemWidth(120);
						if (ImGui::Combo("Mode##lfo", &mode, lfo_mode_labels, 3)) lfo->mMode = static_cast<lx::ELfoMode>(mode);
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
						// Two Noise modulators with the same Seed produce identical values at every
						// (voice, step) -- give each a distinct Seed (auto-assigned on Add Noise) to
						// decorrelate them, e.g. one per R/G/B component of a color.
						ImGui::SetNextItemWidth(80); ImGui::InputInt("Seed", &noise->mSeed);
					}

					int blend = static_cast<int>(m->mBlend);
					ImGui::SetNextItemWidth(100);
					if (ImGui::Combo("Blend", &blend, blend_labels, 3)) m->mBlend = static_cast<lx::EModulatorBlend>(blend);
					ImGui::SameLine(); ImGui::SetNextItemWidth(80); ImGui::DragFloat("Min", &m->mMin, 0.01f, 0.0f, 1.0f);
					ImGui::SameLine(); ImGui::SetNextItemWidth(80); ImGui::DragFloat("Max", &m->mMax, 0.01f, 0.0f, 1.0f);
					ImGui::Separator();
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
				if (ImGui::SmallButton("Remove"))
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
					if (ImGui::Button("+ Add Binding") && !sel.empty())
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


	void lxcontrolApp::drawProgramsTab()
	{
		ImGui::InputText("Name##prog", mNewProgramName, sizeof(mNewProgramName));
		ImGui::SameLine();
		if (ImGui::Button("+ New Program") && std::strlen(mNewProgramName) > 0)
		{
			mLxControlService->createProgram(mNewProgramName);
			mNewProgramName[0] = '\0';
		}
		ImGui::Separator();

		const auto& triggers = mLxControlService->getTriggers();
		const auto& controls = mLxControlService->getControls();
		lx::Program* active = mLxControlService->getActiveProgram();

		// Control-kind triggers only offer themselves in the per-Control mapping combos
		// (Enter/Exit auto-fire on load/unload and aren't manually mappable).
		std::vector<lx::Trigger*> control_triggers;
		for (auto& t : triggers)
			if (t->mKind == lx::ETriggerKind::Control) control_triggers.emplace_back(t.get());

		for (auto& prog : mLxControlService->getPrograms())
		{
			// Keyed by mID, not the raw pointer: any mapping/binding/lifecycle edit below rewrites
			// user_content.json, which the ResourceManager's directory watch hot-reloads next frame,
			// recreating changed Programs/Triggers/Controls at a new address. mID survives that;
			// a pointer-based ID would orphan this tree's open/closed state and it'd appear to collapse.
			ImGui::PushID(prog->mID.c_str());
			bool is_active = (active == prog.get());
			ImGui::Text("%s%s", prog->mName.c_str(), is_active ? "  (active)" : "");
			ImGui::SameLine();
			if (is_active) { if (ImGui::SmallButton("Unload")) mLxControlService->unloadProgram(); }
			else { if (ImGui::SmallButton("Load")) mLxControlService->loadProgram(prog.get()); }
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete")) { mLxControlService->removeProgram(prog.get()); ImGui::PopID(); break; }

			// --- Control Mappings: manage each ControlTrigger (fire/stop/delete + bindings) and
			// pick which Control(s) fire it while this Program is active ---
			if (ImGui::TreeNode("Control Mappings"))
			{
				// Controls already mapped to ANY Trigger in this Program. A Control may only
				// drive one Trigger per Program - setControlMapping enforces this by clearing any
				// existing mapping for that Control first (lxcontrolservice.cpp) - so this list has
				// at most one entry per Control. Used below to scope each row's Combo and to decide
				// whether "+ Add Control binding" has anything left to offer.
				std::vector<lx::Control*> mapped_in_program;
				for (auto& m : prog->mControlMappings)
					if (m->mControl != nullptr)
						mapped_in_program.emplace_back(m->mControl.get());

				for (auto* t : control_triggers)
				{
					ImGui::PushID(t->mID.c_str());

					bool active_t = mLxControlService->isTriggerActive(*t);
					ImGui::BulletText("%s%s", t->mName.c_str(), active_t ? " (active)" : "");
					ImGui::SameLine(); if (ImGui::SmallButton("Fire")) mLxControlService->fireTrigger(*t);
					ImGui::SameLine(); if (ImGui::SmallButton("Stop")) mLxControlService->stopTrigger(*t);
					ImGui::SameLine();
					if (ImGui::SmallButton("Delete##trig"))
					{
						mLxControlService->removeTrigger(t);
						ImGui::PopID();
						break;	// breaks this inner loop only; the program row below still runs
					}

					drawTriggerBindingsEditor(*t);

					// --- Control bindings for this Trigger: one flat row per bound Control
					// (Combo to retarget + Remove), plus an Add button. Replaces the old "Mapped
					// Controls" nested checkbox tree (Program -> Trigger -> checkbox-per-Control).
					if (controls.empty())
					{
						ImGui::TextDisabled("Create a Control in the MIDI tab first");
					}
					else
					{
						// Rows = this Program's ControlMappings targeting this Trigger. PushID'd by
						// the mapping's own mID, not loop index (row order isn't stable across a
						// retarget/remove) and not the Control pointer (this tab hot-reloads from
						// JSON and reallocates objects next frame).
						std::vector<lx::ControlMapping*> rows;
						for (auto& m : prog->mControlMappings)
							if (m->mTrigger.get() == t)
								rows.emplace_back(m.get());

						bool mutated = false;
						for (auto* row : rows)
						{
							ImGui::PushID(row->mID.c_str());
							lx::Control* cur = row->mControl.get();

							// This row's choices = Controls unmapped anywhere in this Program, plus
							// its own current Control (so it stays visible/selected even though
							// every other row treats it as "taken").
							std::vector<lx::Control*> avail;
							std::vector<const char*> avail_labels;
							int cur_idx = 0;
							for (auto& c : controls)
							{
								bool taken = std::find(mapped_in_program.begin(), mapped_in_program.end(), c.get()) != mapped_in_program.end();
								if (taken && c.get() != cur)
									continue;
								if (c.get() == cur)
									cur_idx = static_cast<int>(avail.size());
								avail.emplace_back(c.get());
								avail_labels.emplace_back(c->mName.c_str());
							}

							ImGui::SetNextItemWidth(160);
							if (ImGui::Combo("Control", &cur_idx, avail_labels.data(), static_cast<int>(avail_labels.size())))
							{
								lx::Control* picked = avail[cur_idx];
								if (picked != cur && cur != nullptr)
								{
									// Re-target this voice: vacate the old Control, then bind the new
									// one to the same Trigger. Clearing `cur` is required in addition to
									// setControlMapping on `picked` - otherwise `cur` is left as an
									// orphaned extra binding to `t`.
									mLxControlService->clearControlMapping(*prog.get(), *cur);
									mLxControlService->setControlMapping(*prog.get(), *picked, t);
									mutated = true;
								}
							}

							ImGui::SameLine();
							if (ImGui::SmallButton("Remove##ctrlmap") && cur != nullptr)
							{
								mLxControlService->clearControlMapping(*prog.get(), *cur);
								mutated = true;
							}

							ImGui::PopID();
							if (mutated) break;	// rows / mapped_in_program are now stale; next frame recomputes
						}

						if (!mutated)
						{
							lx::Control* next_avail = nullptr;
							for (auto& c : controls)
							{
								if (std::find(mapped_in_program.begin(), mapped_in_program.end(), c.get()) == mapped_in_program.end())
								{
									next_avail = c.get();
									break;
								}
							}

							// Vendored ImGui is 1.76, predates BeginDisabled/EndDisabled (1.83), so
							// gray manually and gate the click on can_add.
							bool can_add = (next_avail != nullptr);
							if (!can_add) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
							bool add_clicked = ImGui::Button("+ Add Control binding");
							if (!can_add) ImGui::PopStyleVar();
							if (!can_add && ImGui::IsItemHovered())
								ImGui::SetTooltip("Every Control is already mapped to a Trigger in this Program");
							if (add_clicked && can_add)
								mLxControlService->setControlMapping(*prog.get(), *next_avail, t);
						}
					}

					ImGui::Separator();
					ImGui::PopID();
				}

				drawTriggerCreationForm(*prog.get());

				ImGui::TreePop();
			}

			// --- Lifecycle Triggers: Enter/Exit, auto-fired on Program load/unload ---
			if (ImGui::TreeNode("Lifecycle Triggers"))
			{
				for (auto& t : triggers)
				{
					if (t->mKind != lx::ETriggerKind::Enter && t->mKind != lx::ETriggerKind::Exit)
						continue;

					ImGui::PushID(t->mID.c_str());

					bool member = false;
					for (auto& pt : prog->mLifecycleTriggers)
						if (pt.get() == t.get()) { member = true; break; }
					if (ImGui::Checkbox(t->mName.c_str(), &member))
					{
						auto list = prog->mLifecycleTriggers;
						if (member)
						{
							list.emplace_back(nap::ResourcePtr<lx::Trigger>(t.get()));
						}
						else
						{
							list.erase(std::remove_if(list.begin(), list.end(),
								[&t](const nap::ResourcePtr<lx::Trigger>& x) { return x.get() == t.get(); }), list.end());
						}
						mLxControlService->setProgramLifecycleTriggers(*prog.get(), list);
					}

					const char* tn = t->mKind == lx::ETriggerKind::Enter ? "Enter" : "Exit";
					bool active_t = mLxControlService->isTriggerActive(*t.get());
					ImGui::SameLine(); ImGui::TextDisabled("[%s]%s", tn, active_t ? " (active)" : "");
					ImGui::SameLine(); if (ImGui::SmallButton("Fire")) mLxControlService->fireTrigger(*t.get());
					ImGui::SameLine(); if (ImGui::SmallButton("Stop")) mLxControlService->stopTrigger(*t.get());
					ImGui::SameLine();
					if (ImGui::SmallButton("Delete##trig"))
					{
						mLxControlService->removeTrigger(t.get());
						ImGui::PopID();
						break;	// breaks this inner loop only; the program row below still runs
					}

					drawTriggerBindingsEditor(*t.get());

					ImGui::PopID();
				}

				ImGui::Separator();
				drawTriggerCreationForm(*prog.get());

				ImGui::TreePop();
			}
			ImGui::Separator();
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
		if (ImGui::Button("+ New Trigger") && std::strlen(form.mName) > 0)
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

		// Device group (live-editable; re-buckets next frame).
		char gbuf[64];
		std::strncpy(gbuf, c->mGroup.c_str(), sizeof(gbuf) - 1);
		gbuf[sizeof(gbuf) - 1] = '\0';
		ImGui::SameLine(); ImGui::SetNextItemWidth(110);
		if (ImGui::InputText("##group", gbuf, sizeof(gbuf)))
		{
			c->mGroup = gbuf;
			mLxControlService->markDirty();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Device group (blank = ungrouped)");

		// Learn, with an explicit Cancel (Esc also cancels - see inputMessageReceived).
		ImGui::SameLine();
		if (mLearningControl == c)
		{
			ImGui::TextColored(lxtheme::pulse(), "learning...");
			ImGui::SameLine();
			if (ImGui::SmallButton("Cancel"))
				mLearningControl = nullptr;
			else if (mLxControlService->getMidiEventCounter() > mLearnStartCounter)
			{
				MidiEvent ev = mLxControlService->getLastMidiEvent();
				mLxControlService->createBinding(ev, *c);
				mLearningControl = nullptr;
			}
		}
		else if (ImGui::SmallButton("Learn"))
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
			if (ImGui::Button("Cancel"))
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
				if (ImGui::SmallButton("X")) mLxControlService->removeBinding(b.get());
				ImGui::PopID();
			}
		}

		ImGui::PopID();
		return deleted;
	}

	void lxcontrolApp::drawControlsTab()
	{
		// Device list: honest "seen at startup" snapshot (napmidi has no hot-plug enumeration; the Live
		// Bar shows real per-message activity). Never a fake green "connected" (review #3 / findings §6).
		ImGui::TextDisabled("MIDI input ports (startup snapshot):");
		std::string port_names = mMidiPort->getPortNames();
		ImGui::SameLine();
		ImGui::TextUnformatted(port_names.empty() ? "(none)" : port_names.c_str());

		ImGui::BeginChild("MidiLog", ImVec2(0, 96), true);
		for (const auto& line : mLxControlService->getMidiLog())
			ImGui::TextUnformatted(line.c_str());
		ImGui::EndChild();

		// --- New control -------------------------------------------------
		lxtheme::SectionHeader("New Control");
		ImGui::SetNextItemWidth(140);
		ImGui::InputText("Name##ctrl", mNewControlName, sizeof(mNewControlName));
		ImGui::SameLine(); ImGui::SetNextItemWidth(90);
		ImGui::Combo("Mode##ctrl", &mNewControlMode, kModeLabels, 3);
		ImGui::SameLine(); ImGui::SetNextItemWidth(120);
		ImGui::InputText("Device##ctrl", mNewControlGroup, sizeof(mNewControlGroup));
		ImGui::SameLine();
		if (ImGui::Button("+ Add") && std::strlen(mNewControlName) > 0)
		{
			lx::Control* nc = mLxControlService->createControl(mNewControlName, static_cast<lx::EControlMode>(mNewControlMode));
			if (nc != nullptr) { nc->mGroup = mNewControlGroup; mLxControlService->markDirty(); }
			mNewControlName[0] = '\0';
		}

		// --- Controls grouped by device ----------------------------------
		const auto& controls = mLxControlService->getControls();
		if (controls.empty())
		{
			ImGui::Spacing();
			ImGui::TextDisabled("No Controls yet. Add one above, then Learn a MIDI message to bind it.");
			return;
		}

		// Distinct groups in first-seen order; empty group ("(ungrouped)") drawn last.
		std::vector<std::string> groups;
		bool has_ungrouped = false;
		for (auto& c : controls)
		{
			if (c->mGroup.empty()) { has_ungrouped = true; continue; }
			if (std::find(groups.begin(), groups.end(), c->mGroup) == groups.end())
				groups.emplace_back(c->mGroup);
		}
		if (has_ungrouped) groups.emplace_back(std::string());

		for (const auto& g : groups)
		{
			lxtheme::SectionHeader(g.empty() ? "(ungrouped)" : g.c_str());
			for (auto& c : controls)
			{
				if (c->mGroup != g) continue;
				if (drawControlRow(c.get()))
					return;	// deleted -> `controls` mutated, bail this frame
			}
		}
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
