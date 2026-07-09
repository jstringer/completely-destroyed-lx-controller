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
#include <parametergui.h>
#include <parametergroup.h>
#include <lxcontrolservice.h>
#include <midiport/midiinputport.h>
#include <set>
#include <map>

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
		/**
		 * Constructor
		 * @param core instance of the NAP core system
		 */
		lxcontrolApp(nap::Core& core) : App(core) { }

		/**
		 * Initialize all the services and app specific data structures
		 * @param error contains the error code when initialization fails
		 * @return if initialization succeeded
		*/
		bool init(utility::ErrorState& error) override;

		/**
		 * Update is called every frame, before render.
		 * @param deltaTime the time in seconds between calls
		 */
		void update(double deltaTime) override;

		/**
		 * Render is called after update. Use this call to render objects to a specific target
		 */
		void render() override;

		/**
		 * Called when the app receives a window message.
		 * @param windowEvent the window message that occurred
		 */
		void windowMessageReceived(WindowEventPtr windowEvent) override;

		/**
		 * Called when the app receives an input message (from a mouse, keyboard etc.)
		 * @param inputEvent the input event that occurred
		 */
		void inputMessageReceived(InputEventPtr inputEvent) override;

		/**
		 * Called when the app is shutting down after quit() has been invoked
		 * @return the application exit code, this is returned when the main loop is exited
		 */
		virtual int shutdown() override;

	private:
		void drawMainUI();
		void drawPresetsTab();
		void drawEffectsTab();
		void drawMidiTab();
		void drawFixtureParamGroup(ParameterGroup& group);

		/**
		 * Draws a labeled combo box over the given effect layers, returning the newly-selected
		 * index (or -1 for "None"). 'current' is used to determine the initially-selected entry.
		 */
		static int drawEffectLayerCombo(const char* label, const std::vector<rtti::ObjectPtr<EffectLayer>>& layers, EffectLayer* current);

		ResourceManager*			mResourceManager = nullptr;		///< Manages all the loaded data
		RenderService*				mRenderService = nullptr;		///< Render Service that handles render calls
		SceneService*				mSceneService = nullptr;		///< Manages all the objects in the scene
		InputService*				mInputService = nullptr;		///< Input service for processing input
		IMGuiService*				mGuiService = nullptr;			///< Manages GUI related update / draw calls
		lxcontrolService*			mLxControlService = nullptr;	///< Runtime Preset/Effect/MIDI authority
		ObjectPtr<RenderWindow>		mRenderWindow;					///< Pointer to the render window
		ObjectPtr<Scene>			mScene = nullptr;				///< Pointer to the main scene
		ObjectPtr<EntityInstance>	mCameraEntity = nullptr;		///< Pointer to the entity that holds the perspective camera
		ObjectPtr<EntityInstance>	mGnomonEntity = nullptr;		///< Pointer to the entity that can render the gnomon

		ObjectPtr<ParameterGroup>	mFixtureParams1 = nullptr;		///< Fixture 1 parameter group
		ObjectPtr<ParameterGroup>	mFixtureParams2 = nullptr;		///< Fixture 2 parameter group
		ObjectPtr<ParameterGroup>	mFixtureParams3 = nullptr;		///< Fixture 3 parameter group

		// Effect-creation form state
		char						mNewEffectName[128] = "";
		int							mNewEffectPriority = 0;
		std::vector<bool>			mNewEffectTargets;		///< one flag per (fixture, channel) row shown in the form

		// Preset-creation form state
		char						mNewPresetName[128] = "";

		// MIDI-learn workflow state
		bool						mLearning = false;
		int							mLearnStartCounter = 0;
		EMidiMappingTargetKind		mLearnKind = EMidiMappingTargetKind::Parameter;
		int							mLearnParameterIndex = 0;		///< index into a flattened (fixture, channel) list
		int							mLearnPresetIndex = 0;
		float						mLearnInputMinimum = 0.0f;
		float						mLearnInputMaximum = 127.0f;

		// Per-preset "add a new note mapping" form state
		struct NoteMappingForm
		{
			int mNumber = 60;
			int mEffectIndex = 0;
			int mAction = static_cast<int>(EMidiTriggerAction::Toggle);
		};
		std::map<Preset*, NoteMappingForm>	mNoteMappingForms;

		// MIDI device list: the ports our statically-declared MidiInputPort has opened. lxcontrolService
		// polls for newly connected/removed devices and restarts this port to pick them up - see
		// lxcontrolService::update()/MidiHotplugMonitor - so this reflects live state, refreshed every
		// couple of seconds, not just what was present at app startup.
		ObjectPtr<MidiInputPort>	mMidiPort;

		// Effect timelines the user has toggled open; kept open across tab switches
		std::set<EffectLayer*>		mOpenTimelines;
	};
}
