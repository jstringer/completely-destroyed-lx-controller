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
#include <algorithm>

// lx effect/modulator types (for RTTI_OF dispatch + casts)
#include <channelrole.h>
#include <effectparameter.h>
#include <adsrmodulator.h>
#include <admodulator.h>
#include <lfomodulator.h>
#include <stepmodulator.h>
#include <chasemodulator.h>
#include <noisemodulator.h>
#include <trigger.h>
#include <controller.h>
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

		// Active-program banner (shown on every tab)
		lx::Program* active = mLxControlService->getActiveProgram();
		if (active != nullptr)
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Program: %s", active->mName.c_str());
		else
			ImGui::TextDisabled("Program: (none loaded - output is dark)");
		ImGui::Separator();

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
			if (ImGui::BeginTabItem("Programs"))
			{
				drawProgramsTab();
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
				// --- Target mode: how many independent fixture slots this effect computes ---
				static const char* target_mode_labels[] = { "Single Fixture", "Multiple Fixtures" };
				int mode = static_cast<int>(effect->mTargetMode);
				int fixture_count = effect->mFixtureCount;
				bool mode_changed = false;
				ImGui::SetNextItemWidth(160);
				mode_changed |= ImGui::Combo("Target", &mode, target_mode_labels, 2);
				if (mode == static_cast<int>(lx::EEffectTargetMode::Multiple))
				{
					ImGui::SameLine();
					ImGui::SetNextItemWidth(80);
					mode_changed |= ImGui::InputInt("Fixture Count", &fixture_count);
					fixture_count = nap::math::clamp(fixture_count, 1, 32);
				}
				if (mode_changed)
					mLxControlService->setEffectTargetMode(*effect.get(), static_cast<lx::EEffectTargetMode>(mode), fixture_count);

				ImGui::Separator();

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
				ImGui::SameLine(); if (ImGui::Button("Add AD"))   add_mod(RTTI_OF(lx::AdModulator));
				ImGui::SameLine(); if (ImGui::Button("Add LFO"))  add_mod(RTTI_OF(lx::LfoModulator));
				ImGui::SameLine(); if (ImGui::Button("Add Step")) add_mod(RTTI_OF(lx::StepModulator));
				ImGui::SameLine(); if (ImGui::Button("Add Chase")) add_mod(RTTI_OF(lx::ChaseModulator));
				ImGui::SameLine(); if (ImGui::Button("Add Noise")) add_mod(RTTI_OF(lx::NoiseModulator));

				static const char* blend_labels[]    = { "Replace", "Multiply", "Add" };
				static const char* lfo_mode_labels[] = { "Loop", "OneShot", "LoopRetrigger" };
				static const char* ad_mode_labels[]  = { "OneShot", "LoopWhileSustained" };

				for (auto& m : effect->mModulators)
				{
					ImGui::PushID(m.get());

					// Chase/Noise drive a distinct value per fixture slot -- a single scalar plot/bar
					// doesn't represent that, so show one mini progress bar per slot instead.
					bool is_slot_mod = rtti_cast<lx::ChaseModulator>(m.get()) != nullptr || rtti_cast<lx::NoiseModulator>(m.get()) != nullptr;
					if (is_slot_mod)
					{
						int slots = effect->mTargetMode == lx::EEffectTargetMode::Multiple ?
							nap::math::clamp(effect->mFixtureCount, 1, 32) : 1;
						for (int s = 0; s < slots; ++s)
						{
							ImGui::PushID(s);
							ImGui::ProgressBar(m->valueForSlot(s), ImVec2(50, 0));
							ImGui::PopID();
							if (s + 1 < slots) ImGui::SameLine();
						}
					}
					else
					{
						// Live shape plot: raw 0..1 sink value over recent frames.
						auto& hist = mModHistory[m.get()];
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
						ImGui::SetNextItemWidth(100); ImGui::SliderFloat("Smoothing", &noise->mSmoothing, 0.0f, 1.0f);
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
		const auto& effects = mLxControlService->getEffects();
		const auto& fixtures = mLxControlService->getFixtures();

		if (ImGui::TreeNode("Bindings"))
		{
			int bi = 0;
			bool removed = false;
			for (auto& b : trigger.mBindings)
			{
				ImGui::PushID(bi);
				std::string fx;
				for (auto& f : b.mFixtureNames) { fx += f; fx += " "; }
				ImGui::BulletText("%s -> %s", b.mEffect != nullptr ? b.mEffect->mName.c_str() : "(none)", fx.c_str());
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
				if (!effects.empty())
				{
					int& eidx = mBindEffectIdx[trigger.mID];
					std::vector<const char*> elabels;
					for (auto& e : effects) elabels.emplace_back(e->mName.c_str());
					eidx = nap::math::clamp(eidx, 0, static_cast<int>(elabels.size()) - 1);
					ImGui::SetNextItemWidth(160);
					ImGui::Combo("Effect", &eidx, elabels.data(), static_cast<int>(elabels.size()));

					auto& sel = mBindFixtures[trigger.mID];
					if (effects[eidx]->mTargetMode == lx::EEffectTargetMode::Multiple)
					{
						int needed = effects[eidx]->mFixtureCount;
						ImGui::SameLine();
						if (static_cast<int>(sel.size()) == needed)
							ImGui::TextDisabled("needs %d fixtures, %d selected", needed, static_cast<int>(sel.size()));
						else if (static_cast<int>(sel.size()) > needed)
							ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "needs %d fixtures, %d selected -- will repeat", needed, static_cast<int>(sel.size()));
						else
							ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "needs %d fixtures, %d selected -- some slots stay at base", needed, static_cast<int>(sel.size()));
					}

					for (auto* f : fixtures)
					{
						ImGui::PushID(f);
						std::string eid = f->getEntityID();
						bool checked = sel.count(eid) > 0;
						if (ImGui::Checkbox(f->getDisplayName().c_str(), &checked))
						{
							if (checked) sel.insert(eid); else sel.erase(eid);
						}
						ImGui::SameLine();
						ImGui::PopID();
					}
					ImGui::NewLine();
					if (ImGui::Button("+ Add Binding") && !sel.empty())
					{
						auto bindings = trigger.mBindings;
						lx::EffectFixtureBinding nb;
						nb.mEffect = effects[eidx].get();
						for (auto& s : sel) nb.mFixtureNames.emplace_back(s);
						bindings.emplace_back(nb);
						mLxControlService->setTriggerBindings(trigger, bindings);
						sel.clear();
					}
				}
				else
				{
					ImGui::TextDisabled("Create an effect first");
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
		const auto& controllers = mLxControlService->getControllers();
		lx::Program* active = mLxControlService->getActiveProgram();

		// ControllerTrigger-typed triggers only offer themselves in the per-Control mapping combos
		// (Enter/Exit auto-fire on load/unload and aren't manually mappable).
		std::vector<lx::Trigger*> controller_triggers;
		for (auto& t : triggers)
			if (t->get_type() == RTTI_OF(lx::ControllerTrigger)) controller_triggers.emplace_back(t.get());

		for (auto& prog : mLxControlService->getPrograms())
		{
			// Keyed by mID, not the raw pointer: any mapping/binding/lifecycle edit below rewrites
			// user_content.json, which the ResourceManager's directory watch hot-reloads next frame,
			// recreating changed Programs/Triggers/Controllers at a new address. mID survives that;
			// a pointer-based ID would orphan this tree's open/closed state and it'd appear to collapse.
			ImGui::PushID(prog->mID.c_str());
			bool is_active = (active == prog.get());
			ImGui::Text("%s%s", prog->mName.c_str(), is_active ? "  (active)" : "");
			ImGui::SameLine();
			if (is_active) { if (ImGui::SmallButton("Unload")) mLxControlService->unloadProgram(); }
			else { if (ImGui::SmallButton("Load")) mLxControlService->loadProgram(prog.get()); }
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete")) { mLxControlService->removeProgram(prog.get()); ImGui::PopID(); break; }

			// --- Controller Mappings: manage each ControllerTrigger (fire/stop/delete + bindings) and
			// pick which Controller(s) fire it while this Program is active ---
			if (ImGui::TreeNode("Controller Mappings"))
			{
				// Controllers already mapped to ANY Trigger in this Program. A Controller may only
				// drive one Trigger per Program - setControllerMapping enforces this by clearing any
				// existing mapping for that Controller first (lxcontrolservice.cpp) - so this list has
				// at most one entry per Controller. Used below to scope each row's Combo and to decide
				// whether "+ Add Controller binding" has anything left to offer.
				std::vector<lx::Controller*> mapped_in_program;
				for (auto& m : prog->mControllerMappings)
					if (m->mController != nullptr)
						mapped_in_program.emplace_back(m->mController.get());

				for (auto* t : controller_triggers)
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

					// --- Controller bindings for this Trigger: one flat row per bound Controller
					// (Combo to retarget + Remove), plus an Add button. Replaces the old "Mapped
					// Controllers" nested checkbox tree (Program -> Trigger -> checkbox-per-Controller).
					if (controllers.empty())
					{
						ImGui::TextDisabled("Create a Controller in the MIDI tab first");
					}
					else
					{
						// Rows = this Program's ControllerMappings targeting this Trigger. PushID'd by
						// the mapping's own mID, not loop index (row order isn't stable across a
						// retarget/remove) and not the Controller pointer (this tab hot-reloads from
						// JSON and reallocates objects next frame).
						std::vector<lx::ControllerMapping*> rows;
						for (auto& m : prog->mControllerMappings)
							if (m->mTrigger.get() == t)
								rows.emplace_back(m.get());

						bool mutated = false;
						for (auto* row : rows)
						{
							ImGui::PushID(row->mID.c_str());
							lx::Controller* cur = row->mController.get();

							// This row's choices = Controllers unmapped anywhere in this Program, plus
							// its own current Controller (so it stays visible/selected even though
							// every other row treats it as "taken").
							std::vector<lx::Controller*> avail;
							std::vector<const char*> avail_labels;
							int cur_idx = 0;
							for (auto& c : controllers)
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
							if (ImGui::Combo("Controller", &cur_idx, avail_labels.data(), static_cast<int>(avail_labels.size())))
							{
								lx::Controller* picked = avail[cur_idx];
								if (picked != cur && cur != nullptr)
								{
									// Re-target this slot: vacate the old Controller, then bind the new
									// one to the same Trigger. Clearing `cur` is required in addition to
									// setControllerMapping on `picked` - otherwise `cur` is left as an
									// orphaned extra binding to `t`.
									mLxControlService->clearControllerMapping(*prog.get(), *cur);
									mLxControlService->setControllerMapping(*prog.get(), *picked, t);
									mutated = true;
								}
							}

							ImGui::SameLine();
							if (ImGui::SmallButton("Remove##ctrlmap") && cur != nullptr)
							{
								mLxControlService->clearControllerMapping(*prog.get(), *cur);
								mutated = true;
							}

							ImGui::PopID();
							if (mutated) break;	// rows / mapped_in_program are now stale; next frame recomputes
						}

						if (!mutated)
						{
							lx::Controller* next_avail = nullptr;
							for (auto& c : controllers)
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
							bool add_clicked = ImGui::Button("+ Add Controller binding");
							if (!can_add) ImGui::PopStyleVar();
							if (!can_add && ImGui::IsItemHovered())
								ImGui::SetTooltip("Every Controller is already mapped to a Trigger in this Program");
							if (add_clicked && can_add)
								mLxControlService->setControllerMapping(*prog.get(), *next_avail, t);
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
					if (t->get_type() != RTTI_OF(lx::EnterTrigger) && t->get_type() != RTTI_OF(lx::ExitTrigger))
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

					const char* tn = t->get_type() == RTTI_OF(lx::EnterTrigger) ? "Enter" : "Exit";
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
		static const char* type_labels[] = { "Controller", "Enter", "Exit" };

		auto& form = mNewTriggerFormByProgram[program.mID];
		ImGui::InputText("Name##newtrig", form.mName, sizeof(form.mName));
		ImGui::SameLine(); ImGui::SetNextItemWidth(120);
		ImGui::Combo("Type##newtrig", &form.mType, type_labels, 3);
		ImGui::SameLine();
		if (ImGui::Button("+ New Trigger") && std::strlen(form.mName) > 0)
		{
			nap::rtti::TypeInfo type = form.mType == 1 ? RTTI_OF(lx::EnterTrigger)
				: form.mType == 2 ? RTTI_OF(lx::ExitTrigger)
				: RTTI_OF(lx::ControllerTrigger);
			auto* new_trig = mLxControlService->createTrigger(type, form.mName);
			// Enter/Exit triggers auto-join this Program's lifecycle list (one-step workflow);
			// ControllerTriggers are created unassigned - map them via the Controller Mappings menu.
			if (form.mType == 1 || form.mType == 2)
			{
				auto list = program.mLifecycleTriggers;
				list.emplace_back(nap::ResourcePtr<lx::Trigger>(new_trig));
				mLxControlService->setProgramLifecycleTriggers(program, list);
			}
			form.mName[0] = '\0';
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
		ImGui::BeginChild("MidiLog", ImVec2(0, 120), true);
		for (const auto& line : mLxControlService->getMidiLog())
			ImGui::TextUnformatted(line.c_str());
		ImGui::EndChild();

		ImGui::Separator();
		ImGui::Text("Controllers:");

		static const char* mode_labels[] = { "Momentary", "Toggle", "FireOnly" };

		ImGui::InputText("Name##ctrl", mNewControllerName, sizeof(mNewControllerName));
		ImGui::SameLine(); ImGui::SetNextItemWidth(110);
		ImGui::Combo("Mode##ctrl", &mNewControllerMode, mode_labels, 3);
		ImGui::SameLine();
		if (ImGui::Button("+ New Controller") && std::strlen(mNewControllerName) > 0)
		{
			mLxControlService->createController(mNewControllerName, static_cast<lx::EControllerMode>(mNewControllerMode));
			mNewControllerName[0] = '\0';
		}

		ImGui::Separator();
		for (auto& c : mLxControlService->getControllers())
		{
			ImGui::PushID(c.get());
			int mode = static_cast<int>(c->mMode);
			ImGui::Text("%s", c->mName.c_str());
			ImGui::SameLine(); ImGui::SetNextItemWidth(110);
			if (ImGui::Combo("##mode", &mode, mode_labels, 3)) c->mMode = static_cast<lx::EControllerMode>(mode);

			ImGui::SameLine();
			if (mLearningController == c.get())
			{
				ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "learning...");
				if (mLxControlService->getMidiEventCounter() > mLearnStartCounter)
				{
					MidiEvent ev = mLxControlService->getLastMidiEvent();
					mLxControlService->createBinding(ev, *c.get());
					mLearningController = nullptr;
				}
			}
			else if (ImGui::SmallButton("Learn"))
			{
				mLearningController = c.get();
				mLearnStartCounter = mLxControlService->getMidiEventCounter();
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete")) { mLxControlService->removeController(c.get()); ImGui::PopID(); break; }

			for (auto& b : mLxControlService->getBindings())
			{
				if (b->mController.get() != c.get())
					continue;
				ImGui::PushID(b.get());
				std::string nums;
				for (int n : b->mNumbers) { nums += std::to_string(n); nums += " "; }
				ImGui::BulletText("num: %s", nums.c_str());
				ImGui::SameLine();
				if (ImGui::SmallButton("X")) mLxControlService->removeBinding(b.get());
				ImGui::PopID();
			}
			ImGui::PopID();
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
