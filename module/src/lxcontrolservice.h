#pragma once

// External Includes
#include <nap/service.h>
#include <nap/resourceptr.h>
#include <nap/signalslot.h>
#include <midievent.h>
#include <midiinputcomponent.h>
#include <midiport/midiinputport.h>
#include <sequenceplayer.h>
#include <sequenceplayerclock.h>
#include <sequenceeditor.h>
#include <sequenceplayercurveoutput.h>
#include <parameternumeric.h>
#include <mathutils.h>
#include <deque>
#include <vector>

// Local Includes
#include "midihotplugmonitor.h"
#include "effect.h"
#include "trigger.h"
#include "controller.h"
#include "midibinding.h"
#include "program.h"
#include <cstdint>

namespace lx
{
	class FixtureComponentInstance;

	/** One keyframe for lxcontrolService::authorFloatCurve: absolute time (seconds), value 0..1, and the
	 *  interpolation used to reach this point from the previous one. */
	struct Key
	{
		double					mTime = 0.0;
		float					mValue = 0.0f;
		nap::math::ECurveInterp	mInterp = nap::math::ECurveInterp::Linear;
	};
}

namespace nap
{
	class ResourceManager;

	/**
	 * Runtime authority for the lxcontrol app.
	 *
	 * Owns the fixture registry, the wildcard MIDI-listener subscription + hot-plug reconnect + learn
	 * snapshot, and (Phase 2) the live-authored Effects: their EffectParameters and Modulators, each
	 * modulator backed by its own SequencePlayer + clock + stock SequencePlayerCurveOutput (curve engine).
	 * Persists everything to data/user_content.json.
	 */
	class NAPAPI lxcontrolService : public Service
	{
		RTTI_ENABLE(Service)
	public:
		lxcontrolService(ServiceConfiguration* configuration) : Service(configuration) { }

		virtual void getDependentServices(std::vector<rtti::TypeInfo>& dependencies) override;
		virtual void registerObjectCreators(rtti::Factory& factory) override;
		virtual bool init(utility::ErrorState& errorState) override;
		virtual void update(double deltaTime) override;
		virtual void shutdown() override;

		/** Connects to the wildcard MIDI listener, remembers the port, and loads any authored content. */
		bool setup(MidiInputComponentInstance& midiSource, ResourcePtr<MidiInputPort> midiPort, utility::ErrorState& errorState);

		// --- Fixture registry (fixtures self-register from their component init) ---
		void registerFixture(lx::FixtureComponentInstance* fixture);
		void unregisterFixture(lx::FixtureComponentInstance* fixture);
		const std::vector<lx::FixtureComponentInstance*>& getFixtures() const { return mFixtures; }

		// --- Effects ---
		lx::Effect* createEffect(const std::string& name);
		lx::EffectParameter* addEffectParameter(lx::Effect& effect, rtti::TypeInfo type);
		lx::Modulator* addModulator(lx::Effect& effect, rtti::TypeInfo type, lx::EffectParameter* target);
		void removeEffect(lx::Effect* effect);
		const std::vector<rtti::ObjectPtr<lx::Effect>>& getEffects() const { return mEffects; }

		/**
		 * Authors a float curve track at runtime from a keyframe list, via the editor's SequenceControllerCurve.
		 * Clears any existing (auto-created) segments, lays one contiguous segment per interval [t_i, t_{i+1}],
		 * sets values (0..1) + per-point interpolation, and sets the sequence duration to the last key time.
		 * Main-thread only (StandardClock keeps authoring and playback from racing). This is the shared engine
		 * used by every modulator shape's generateCurve().
		 */
		void authorFloatCurve(SequenceEditor& editor, const std::string& trackID, const std::vector<lx::Key>& keys);

		// --- Triggers ---
		lx::Trigger* createTrigger(rtti::TypeInfo type, const std::string& name);
		void setTriggerBindings(lx::Trigger& trigger, const std::vector<lx::EffectFixtureBinding>& bindings);
		void removeTrigger(lx::Trigger* trigger);
		const std::vector<rtti::ObjectPtr<lx::Trigger>>& getTriggers() const { return mTriggers; }
		uint64_t fireTrigger(lx::Trigger& trigger);
		void stopTrigger(lx::Trigger& trigger);
		bool isTriggerActive(lx::Trigger& trigger) const;

		// --- Controllers + MIDI bindings ---
		lx::Controller* createController(const std::string& name, lx::Trigger* trigger, lx::EControllerMode mode);
		lx::MidiBinding* createBinding(const MidiEvent& learnedEvent, lx::Controller& controller);
		void removeController(lx::Controller* controller);
		void removeBinding(lx::MidiBinding* binding);
		const std::vector<rtti::ObjectPtr<lx::Controller>>& getControllers() const { return mControllers; }
		const std::vector<rtti::ObjectPtr<lx::MidiBinding>>& getBindings() const { return mBindings; }

		// --- Programs ---
		lx::Program* createProgram(const std::string& name);
		void setProgramTriggers(lx::Program& program, const std::vector<rtti::ObjectPtr<lx::Trigger>>& triggers);
		void removeProgram(lx::Program* program);
		void loadProgram(lx::Program* program);
		void unloadProgram();
		lx::Program* getActiveProgram() const { return mActiveProgram; }
		const std::vector<rtti::ObjectPtr<lx::Program>>& getPrograms() const { return mPrograms; }

		// --- MIDI log / learn ---
		const std::deque<std::string>& getMidiLog() const { return mMidiLog; }
		bool hasLastMidiEvent() const { return mHasLastEvent; }
		MidiEvent getLastMidiEvent() const { return MidiEvent(mLastEventType, mLastEventNumber, mLastEventValue, mLastEventChannel, mLastEventPort); }
		int getMidiEventCounter() const { return mMidiEventCounter; }

	private:
		struct ModulatorEntry
		{
			rtti::ObjectPtr<lx::Modulator>				mModulator;
			rtti::ObjectPtr<SequencePlayer>				mPlayer;
			rtti::ObjectPtr<SequencePlayerClock>			mClock;
			rtti::ObjectPtr<SequencePlayerCurveOutput>	mOutput;	// stock curve output -> sink parameter
			rtti::ObjectPtr<ParameterFloat>				mSink;
			rtti::ObjectPtr<SequenceEditor>				mEditor;	// runtime curve authoring + duration
		};

		struct EffectEntry
		{
			rtti::ObjectPtr<lx::Effect>						mEffect;
			std::vector<ModulatorEntry>						mModulators;
			std::vector<rtti::ObjectPtr<lx::EffectParameter>>	mParams;
			bool											mRemoved = false;
		};

		struct Activation
		{
			uint64_t					mId = 0;
			lx::Trigger*				mTrigger = nullptr;
			std::vector<lx::Effect*>	mEffects;
			bool						mReleasing = false;
		};

		void onMidiEvent(const MidiEvent& event);
		void save();
		void rebuildFromLoadedContent();
		std::string makeUniqueID(const std::string& base) const;
		EffectEntry* findEntry(lx::Effect& effect);
		bool buildModulatorGraph(ModulatorEntry& entry, const std::string& base, utility::ErrorState& errorState);
		lx::FixtureComponentInstance* findFixture(const std::string& entityID) const;
		void reapClaims(uint64_t activationId);

		ResourceManager*					mResourceManager = nullptr;
		std::vector<lx::FixtureComponentInstance*>	mFixtures;

		ResourcePtr<MidiInputPort>			mMidiPort;
		std::unique_ptr<MidiHotplugMonitor>	mMidiHotplugMonitor;

		std::vector<EffectEntry>				mEffectEntries;
		std::vector<rtti::ObjectPtr<lx::Effect>>	mEffects;	// mirrors mEffectEntries for getEffects()

		std::vector<rtti::ObjectPtr<lx::Trigger>>	mTriggers;
		std::vector<rtti::ObjectPtr<lx::Controller>>	mControllers;
		std::vector<rtti::ObjectPtr<lx::MidiBinding>>	mBindings;
		std::vector<rtti::ObjectPtr<lx::Program>>	mPrograms;
		lx::Program*							mActiveProgram = nullptr;	// runtime, not persisted
		std::vector<Activation>					mActivations;
		uint64_t								mNextActivationId = 1;

		std::deque<std::string>			mMidiLog;
		static constexpr size_t			sMaxMidiLogSize = 50;
		bool							mHasLastEvent = false;
		MidiEvent::Type					mLastEventType = MidiEvent::Type::controlChange;
		MidiValue						mLastEventNumber = 0;
		MidiValue						mLastEventValue = 0;
		MidiValue						mLastEventChannel = 0;
		std::string						mLastEventPort;
		int								mMidiEventCounter = 0;

		Slot<const MidiEvent&> mMidiSlot = { this, &lxcontrolService::onMidiEvent };
	};
}
