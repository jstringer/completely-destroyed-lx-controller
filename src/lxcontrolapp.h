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
		void drawTriggersTab();
		void drawProgramsTab();
		void drawMidiTab();
		void drawFixtureParamGroup(ParameterGroup& group);

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

		// Effects tab form state
		char						mNewEffectName[128] = "";
		std::map<lx::Effect*, int>	mModTargetIndex;	// per-effect selected target-parameter index

		// Triggers tab form state
		char						mNewTriggerName[128] = "";
		int							mNewTriggerType = 0;	// 0=Controller,1=Enter,2=Exit
		std::map<lx::Trigger*, int>					mBindEffectIdx;		// per-trigger add-binding effect selection
		std::map<lx::Trigger*, std::set<std::string>>	mBindFixtures;	// per-trigger add-binding fixture selection

		// MIDI tab form state
		char						mNewControllerName[128] = "";
		int							mNewControllerTriggerIdx = 0;
		int							mNewControllerMode = 0;	// 0=Momentary,1=Toggle,2=FireOnly
		lx::Controller*				mLearningController = nullptr;	// controller awaiting a learned MIDI event
		int							mLearnStartCounter = 0;

		// Programs tab form state
		char						mNewProgramName[128] = "";
	};
}
