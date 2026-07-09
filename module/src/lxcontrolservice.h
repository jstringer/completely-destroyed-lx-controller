#pragma once

// External Includes
#include <nap/service.h>
#include <nap/resourceptr.h>
#include <nap/signalslot.h>
#include <parametergroup.h>
#include <renderwindow.h>
#include <midievent.h>
#include <midiinputcomponent.h>
#include <midiport/midiinputport.h>
#include <sequenceplayer.h>
#include <sequenceeditor.h>
#include <sequenceeditorgui.h>
#include <deque>

// Local Includes
#include "fixture.h"
#include "preset.h"
#include "effectlayer.h"
#include "effectlayeroverride.h"
#include "midimapping.h"
#include "midihotplugmonitor.h"
#include "presetmiditrigger.h"

namespace nap
{
	/**
	 * Runtime authority for GUI-authored Presets, Effects and MIDI mappings.
	 *
	 * objects.json only declares the fixed rig (fixtures, universes, the wildcard MIDI listener).
	 * Everything the user creates live in the Presets/Effects/MIDI tabs - Presets, EffectLayers
	 * (with their SequencePlayer/SequenceEditor/SequenceEditorGUI and per-fixture override
	 * parameters), and MidiMappings - is constructed at runtime via ResourceManager::createObject(),
	 * tracked here, and persisted to data/user_content.json. objects.json itself is never rewritten.
	 */
	class NAPAPI lxcontrolService : public Service
	{
		RTTI_ENABLE(Service)
	public:
		lxcontrolService(ServiceConfiguration* configuration) : Service(configuration)	{ }

		virtual void getDependentServices(std::vector<rtti::TypeInfo>& dependencies) override;
		virtual bool init(nap::utility::ErrorState& errorState) override;
		virtual void update(double deltaTime) override;
		virtual void shutdown() override;

		/**
		 * Called once from lxcontrolApp::init(), after objects.json has loaded. Gathers the fixtures
		 * and their per-fixture parameter groups, connects to the wildcard MIDI listener, and loads
		 * any previously-authored content from data/user_content.json.
		 *
		 * There is intentionally no single merged "all fixtures" ParameterGroup: nap::ParameterGroup's
		 * Groups property is Embedded (must be written inline in JSON), so it can't hold ID-references
		 * to the three already-independently-declared per-fixture groups. A Preset's snapshot is
		 * therefore stored as one file per fixture (see createPreset/applyPreset).
		 */
		bool setup(const std::vector<ResourcePtr<ParameterGroup>>& fixtureParams, ResourcePtr<RenderWindow> renderWindow,
			const std::vector<ResourcePtr<Fixture>>& fixtures, MidiInputComponentInstance& midiSource,
			ResourcePtr<MidiInputPort> midiPort, utility::ErrorState& errorState);

		// --- Fixtures (read-only, declared in objects.json) ---
		const std::vector<Fixture*>& getFixtures() const { return mFixtures; }

		// --- Presets ---
		Preset* createPreset(const std::string& name, utility::ErrorState& errorState);
		void removePreset(Preset* preset);
		void applyPreset(Preset* preset, utility::ErrorState& errorState);
		void resnapshotPreset(Preset* preset, utility::ErrorState& errorState);
		const std::vector<rtti::ObjectPtr<Preset>>& getPresets() const { return mPresets; }

		/**
		 * @return the preset lxcontrolService considers currently active (last one passed to
		 * applyPreset()), or nullptr if none has been activated yet (including right after a
		 * fresh load - this is deliberately not persisted to user_content.json).
		 */
		Preset* getActivePreset() const { return mActivePreset; }

		// --- Preset enter/exit effect triggers ---
		void setPresetOnEnter(Preset* preset, EffectLayer* effect, EMidiTriggerAction action);
		void setPresetOnExit(Preset* preset, EffectLayer* effect, EMidiTriggerAction action);

		// --- Preset-scoped note->effect triggers ---
		PresetMidiTrigger* addPresetNoteMapping(Preset* preset, MidiValue number, EffectLayer* effect,
			EMidiTriggerAction action, utility::ErrorState& errorState);
		void updatePresetNoteMapping(PresetMidiTrigger* trigger, MidiValue number, EffectLayer* effect, EMidiTriggerAction action);
		void removePresetNoteMapping(Preset* preset, PresetMidiTrigger* trigger);

		// --- Effects ---
		struct EffectTarget
		{
			Fixture*	mFixture;
			std::string	mChannelName;
		};

		EffectLayer* createEffectLayer(const std::string& name, int priority, const std::vector<EffectTarget>& targets, utility::ErrorState& errorState);
		void removeEffectLayer(EffectLayer* layer);
		SequenceEditorGUI* getEffectEditorGUI(EffectLayer* layer) const;
		const std::vector<rtti::ObjectPtr<EffectLayer>>& getEffectLayers() const { return mEffectLayers; }

		// --- MIDI ---
		MidiMapping* createMidiMapping(const MidiEvent& learnedEvent, EMidiMappingTargetKind kind,
			ParameterFloat* parameter, float inputMinimum, float inputMaximum,
			Preset* preset, utility::ErrorState& errorState);
		void removeMidiMapping(MidiMapping* mapping);
		const std::vector<rtti::ObjectPtr<MidiMapping>>& getMidiMappings() const { return mMidiMappings; }
		const std::deque<std::string>& getMidiLog() const { return mMidiLog; }

		/**
		 * @return whether a MidiEvent has been received yet
		 */
		bool hasLastMidiEvent() const { return mHasLastEvent; }

		/**
		 * @return a freshly-constructed copy of the most recently received MidiEvent.
		 * Only call when hasLastMidiEvent() is true. Used by the MIDI-learn workflow to
		 * capture the event to build a new MidiMapping from.
		 */
		MidiEvent getLastMidiEvent() const { return MidiEvent(mLastEventType, mLastEventNumber, mLastEventValue, mLastEventChannel, mLastEventPort); }

		/**
		 * @return a counter incremented on every received MidiEvent. Compare against a snapshot taken
		 * when learn-mode started to detect that a new (learnable) event has arrived since.
		 */
		int getMidiEventCounter() const { return mMidiEventCounter; }

	private:
		struct EffectLayerEntry
		{
			rtti::ObjectPtr<EffectLayer>						mLayer;
			rtti::ObjectPtr<SequencePlayer>					mPlayer;
			rtti::ObjectPtr<SequenceEditor>					mEditor;
			rtti::ObjectPtr<SequenceEditorGUI>					mEditorGUI;
			std::vector<rtti::ObjectPtr<ParameterFloat>>		mOverrideParams;
			std::vector<rtti::ObjectPtr<EffectLayerOverride>>	mOverrideLinks;
			bool												mRemoved = false;
		};

		void onMidiEvent(const MidiEvent& event);
		void save();
		void rebuildFromLoadedContent();
		std::string makeUniqueID(const std::string& base) const;
		FixtureChannelBinding* findBinding(Fixture& fixture, const std::string& channelName) const;

		ResourceManager*					mResourceManager = nullptr;
		std::vector<ResourcePtr<ParameterGroup>>	mFixtureParams;		// one per fixture, same order as mFixtures
		ResourcePtr<RenderWindow>			mRenderWindow;
		std::vector<Fixture*>				mFixtures;

		ResourcePtr<MidiInputPort>			mMidiPort;
		std::unique_ptr<MidiHotplugMonitor>	mMidiHotplugMonitor;

		std::vector<rtti::ObjectPtr<Preset>>		mPresets;
		std::vector<EffectLayerEntry>				mEffectEntries;
		std::vector<rtti::ObjectPtr<EffectLayer>>	mEffectLayers;		// mirrors mEffectEntries, kept for getEffectLayers()
		std::vector<rtti::ObjectPtr<MidiMapping>>	mMidiMappings;

		// Currently-active preset. Deliberately NOT persisted/reconstructed - after a fresh
		// load or restart, nothing is "active" until applyPreset() is explicitly called again.
		Preset*										mActivePreset = nullptr;

		std::deque<std::string>			mMidiLog;
		static constexpr size_t			sMaxMidiLogSize = 50;
		bool								mHasLastEvent = false;
		MidiEvent::Type						mLastEventType = MidiEvent::Type::controlChange;
		MidiValue							mLastEventNumber = 0;
		MidiValue							mLastEventValue = 0;
		MidiValue							mLastEventChannel = 0;
		std::string							mLastEventPort;
		int									mMidiEventCounter = 0;

		Slot<const MidiEvent&> mMidiSlot = { this, &lxcontrolService::onMidiEvent };
	};
}
