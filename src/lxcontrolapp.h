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
		void drawFixturesTab();
		void drawEffectsTab();
		void drawTriggerBindingsEditor(lx::Trigger& trigger);
		void drawTriggerCreationForm(lx::Program& program);
		void drawProgramsTab();
		void drawMidiTab();
		void drawFixtureParamGroup(ParameterGroup& group);
		/** Best-effort label for a Multiple-mode effect's fixture slot in the modulator preview: finds any
		 *  Trigger binding targeting this effect and resolves which physically-ordered fixture landed in
		 *  `slot` (mirrors lxcontrolService::fireTrigger's own assignment). Falls back to "Slot N" if no
		 *  binding is found, or if the effect is bound by more than one Trigger with different fixture
		 *  sets (a known shared-Effect-state limitation -- see QUESTIONS.md) the first match wins, so this
		 *  is a preview aid, not a runtime guarantee. */
		std::string describeEffectSlot(lx::Effect* effect, int slot);

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

		// Effects tab form state. Keyed by mID rather than pointer: several service calls (addModulator,
		// addEffectParameter, setEffectTargetMode, ...) call save(), which rewrites user_content.json and
		// gets hot-reloaded by nap::ResourceManager's directory watch, recreating Effects/Modulators at a
		// new address next frame -- a pointer-keyed map would silently orphan its entry (plot history
		// resets, selection resets) on the very next such edit. Same reasoning as mBindEffectIdx/mBindFixtures below.
		char						mNewEffectName[128] = "";
		std::map<std::string, int>	mModTargetIndex;	// per-effect (by mID) selected target-parameter index
		std::map<std::string, std::vector<float>>	mModHistory;	// per-modulator (by mID) live value ring for the shape plot

		// Trigger bindings-editor form state (per-trigger, shared regardless of which Program's section it's
		// viewed from). Keyed by mID rather than pointer: editing bindings rewrites user_content.json, which
		// nap::ResourceManager's directory watch hot-reloads, recreating the changed Trigger/Program at a new
		// address - a pointer-keyed map (or ImGui PushID) would silently orphan its entry / reset tree state
		// on the very next frame.
		std::map<std::string, int>					mBindEffectIdx;		// per-trigger add-binding effect selection
		std::map<std::string, std::set<std::string>>	mBindFixtures;	// per-trigger add-binding fixture selection

		// Trigger creation form state, one per Program section (a user may have several Program sections open at once)
		struct NewTriggerForm
		{
			char	mName[128] = "";
			int		mType = 0;	// 0=Controller,1=Enter,2=Exit
		};
		std::map<std::string, NewTriggerForm> mNewTriggerFormByProgram;	// keyed by Program::mID, see note above

		// MIDI tab form state
		char						mNewControllerName[128] = "";
		int							mNewControllerMode = 0;	// 0=Momentary,1=Toggle,2=FireOnly
		lx::Controller*				mLearningController = nullptr;	// controller awaiting a learned MIDI event
		int							mLearnStartCounter = 0;

		// Programs tab form state
		char						mNewProgramName[128] = "";
	};
}
