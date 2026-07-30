#pragma once

// Core includes
#include <nap/resourcemanager.h>
#include <nap/resourceptr.h>

// Module includes
#include <renderservice.h>
#include <imguiservice.h>
#include <sceneservice.h>
#include <inputservice.h>
#include <scene.h>
#include <renderwindow.h>
#include <entity.h>
#include <app.h>
#include <parametergroup.h>
#include <lxcontrolservice.h>
#include <midiport/midiinputport.h>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace nap
{
	using namespace rtti;

	/**
	 * Main application that is called from within the main loop
	 */
	class lxcontrolApp : public App
	{
		RTTI_ENABLE(App)
	public:
		lxcontrolApp(nap::Core& core) : App(core) { }

		bool init(utility::ErrorState& error) override;
		void update(double deltaTime) override;
		void render() override;
		void windowMessageReceived(WindowEventPtr windowEvent) override;
		void inputMessageReceived(InputEventPtr inputEvent) override;
		virtual int shutdown() override;

	private:
		void drawMainUI();
		/** Persistent top bar (all modes): active program, honest output state (activeVoiceCount),
		 *  ■ All Stop (stopAll), MIDI activity readout, and the Perform/Edit toggle. */
		void drawLiveBar();
		/** Cue-select the previous(-1)/next(+1) program into mCuedProgram without loading it. */
		void cueProgram(int dir);
		/** Perform mode: a play-only pad grid. Each pad fires its Control's mapped Trigger in the
		 *  active program; no authoring/editing. Keeps the Live Bar + output readout visible. */
		void drawPerformGrid();
		void drawRigTab();
		/** Live output strip for one fixture: Dimmer/Strobe filling faders, per-unit RGB swatches, and a
		 *  per-channel output-LED row -- all from FixtureChannelComponentInstance::resolveValue() (the real
		 *  post-arbitration output, i.e. honest output state / C3), not a phantom mix. */
		void drawFixtureOutputStrip(lx::FixtureComponentInstance& fx);
		void drawPatchesTab();
		void drawTriggerBindingsEditor(lx::Trigger& trigger);
		void drawTriggerCreationForm(lx::Program& program);
		/** PROGRAMS panel 1: the programs list (loaded=gold, cued=selected-for-edit) + New. */
		void drawProgramsListPanel();
		/** PROGRAMS panel 2: header + editing band + Routing matrix + Automatic + Output, for one program. */
		void drawRoutingPanel(lx::Program* prog);
		void drawControlsTab();
		/** One Control row in the CONTROLS surface. Returns true if it deleted the control (caller
		 *  must stop iterating the controls vector that frame). */
		bool drawControlRow(lx::Control* c);
		void drawFixtureParamGroup(ParameterGroup& group);
		/** Agent bridge (src/lxagent.h): serve one non-click verb, and the state dump every ack carries. */
		void handleAgentCommand(const std::string& verb, const std::string& arg);
		std::string agentStatus() const;
		/** Best-effort label for a Multiple-mode patch's fixture voice in the modulator preview: finds any
		 *  Trigger binding targeting this patch and resolves which physically-ordered fixture landed in
		 *  `voice` (mirrors lxcontrolService::fireTrigger's own assignment). Falls back to "Voice N" if no
		 *  binding is found, or if the patch is bound by more than one Trigger with different fixture
		 *  sets (a known shared-Patch-state limitation -- see QUESTIONS.md) the first match wins, so this
		 *  is a preview aid, not a runtime guarantee. */
		std::string describePatchVoice(lx::Patch* patch, int voice);

		ResourceManager*			mResourceManager = nullptr;		///< Manages all the loaded data
		RenderService*				mRenderService = nullptr;		///< Render Service that handles render calls
		SceneService*				mSceneService = nullptr;		///< Manages all the objects in the scene
		InputService*				mInputService = nullptr;		///< Input service for processing input
		IMGuiService*				mGuiService = nullptr;			///< Manages GUI related update / draw calls
		lxcontrolService*			mLxControlService = nullptr;	///< Runtime authority (fixtures/MIDI)
		ObjectPtr<RenderWindow>		mRenderWindow;					///< Pointer to the render window
		ObjectPtr<Scene>			mScene = nullptr;				///< Pointer to the main scene
		ObjectPtr<EntityInstance>	mCameraEntity = nullptr;		///< Pointer to the entity that holds the perspective camera
		ObjectPtr<EntityInstance>	mGnomonEntity = nullptr;		///< Pointer to the entity that can render the gnomon

		ObjectPtr<ParameterGroup>	mFixtureParams1 = nullptr;		///< Fixture 1 base-value parameter group
		ObjectPtr<ParameterGroup>	mFixtureParams2 = nullptr;		///< Fixture 2 base-value parameter group
		ObjectPtr<ParameterGroup>	mFixtureParams3 = nullptr;		///< Fixture 3 base-value parameter group

		// The ports our statically-declared MidiInputPort has opened; lxcontrolService restarts this
		// port when devices connect/disconnect (hot-plug), so this reflects live state.
		ObjectPtr<MidiInputPort>	mMidiPort;

		// Patches tab form state. Keyed by mID rather than pointer: several service calls (addModulator,
		// addPatchParameter, setPatchTargetMode, ...) call save(), which rewrites user_content.json and
		// gets hot-reloaded by nap::ResourceManager's directory watch, recreating Patches/Modulators at a
		// new address next frame -- a pointer-keyed map would silently orphan its entry (plot history
		// resets, selection resets) on the very next such edit. Same reasoning as mBindPatchIdx/mBindFixtures below.
		char						mNewPatchName[128] = "";
		std::map<std::string, int>	mModTargetIndex;	// per-patch (by mID) selected target-parameter index

		// Trigger bindings-editor form state (per-trigger, shared regardless of which Program's section it's
		// viewed from). Keyed by mID rather than pointer: editing bindings rewrites user_content.json, which
		// nap::ResourceManager's directory watch hot-reloads, recreating the changed Trigger/Program at a new
		// address - a pointer-keyed map (or ImGui PushID) would silently orphan its entry / reset tree state
		// on the very next frame.
		std::map<std::string, int>					mBindPatchIdx;		// per-trigger add-binding patch selection
		std::map<std::string, std::set<std::string>>	mBindFixtures;	// per-trigger add-binding fixture selection

		// Trigger creation form state, one per Program section (a user may have several Program sections open at once)
		struct NewTriggerForm
		{
			char	mName[128] = "";
			int		mType = 0;	// 0=Control,1=Enter,2=Exit
		};
		std::map<std::string, NewTriggerForm> mNewTriggerFormByProgram;	// keyed by Program::mID, see note above

		// CONTROLS tab form state
		char						mNewControlName[128] = "";
		int							mNewControlMode = 0;	// 0=Hold(Momentary),1=Latch(Toggle),2=Trig(FireOnly)
		char						mNewControlGroup[64] = "";	// device group for the next created control
		lx::Control*				mLearningControl = nullptr;	// control awaiting a learned MIDI event
		int							mLearnStartCounter = 0;

		// Programs tab form state
		char						mNewProgramName[128] = "";

		// Design-language test bed (src/lxstyleguide.cpp). Opt-in overlay; toggled from the Live Bar.
		bool						mShowStyleGuide = false;

		// Perform vs Edit (the Live Bar toggle). Perform = play-only pad grid; Edit = authoring tabs.
		enum class EUiMode { Edit, Perform };
		EUiMode						mMode = EUiMode::Edit;

		// PROGRAMS tab: width of the PATCH EDITOR panel, dragged via the splitter between routing and it.
		float						mPatchPanelWidth = 600.0f;

		// Live Bar / PROGRAMS shared selection: the "cued" (selected-for-edit) program, distinct from the
		// loaded/active one (getActiveProgram). Cue with the bar's prev/next; Load commits it live.
		lx::Program*				mCuedProgram = nullptr;
		// MIDI activity: wall-clock (ImGui::GetTime) of the last seen message, for a "time since" readout.
		double						mLastMidiSeen = -1.0;
		int							mLastMidiCounter = 0;

		// Which authoring tab drew this frame; reported in the agent bridge's state dump (src/lxagent.h).
		std::string					mActiveTab = "RIG";
	};
}
