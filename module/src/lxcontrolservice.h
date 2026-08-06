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
#include <rtti/deserializeresult.h>
#include <parameternumeric.h>
#include <mathutils.h>
#include <deque>
#include <vector>
#include <unordered_set>
#include <functional>

// Local Includes
#include "midihotplugmonitor.h"
#include "fixturegroup.h"
#include "patch.h"
#include "trigger.h"
#include "control.h"
#include "controlmapping.h"
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

	/** One addressable spread target: a fixture, plus which of its colour units (0 for whole-fixture
	 *  roles). Derived from a FixtureGroup at fire time; never authored, never serialized. */
	struct Source
	{
		FixtureComponentInstance*	mFixture = nullptr;
		int							mUnitIndex = 0;
	};
}

namespace nap
{
	class ResourceManager;

	/**
	 * Runtime authority for the lxcontrol app.
	 *
	 * Owns the fixture registry, the wildcard MIDI-listener subscription + hot-plug reconnect + learn
	 * snapshot, and (Phase 2) the live-authored Patches: their PatchParameters and Modulators, each
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
		/** @return all registered fixtures sorted by physical DMX StartChannel (rig order) -- the same
		 *  order fireTrigger uses to assign Chase/Noise fixture voices. Fixture-picking UI should iterate
		 *  this (not getFixtures()'s raw registration order) so what's shown while authoring matches what
		 *  actually happens at fire time. */
		std::vector<lx::FixtureComponentInstance*> getFixturesPhysicalOrder() const;

		// --- Fixture groups (what a Control binds to; ordered whole fixtures) ---
		lx::FixtureGroup* createGroup(const std::string& name);
		void removeGroup(lx::FixtureGroup* group);
		void setGroupFixtures(lx::FixtureGroup& group, const std::vector<std::string>& fixtureIDs);
		const std::vector<rtti::ObjectPtr<lx::FixtureGroup>>& getGroups() const { return mGroups; }
		/** @return the "All Fixtures" group, creating it (every fixture, physical DMX order) if absent.
		 *  Called from setup() so a fresh install always has something bindable, and so the lifecycle
		 *  rows' old "all fixtures" default has a real group to point at. */
		lx::FixtureGroup* ensureDefaultGroup();
		/** Flattens a group into its ordered source list for one spread class. Fixture order follows the
		 *  group's own member order (that is what makes reordering a group reverse a chase); colour units
		 *  ascend within each fixture. Members that no longer exist in the rig are skipped. */
		std::vector<lx::Source> sourcesFor(const lx::FixtureGroup& group, lx::ESpreadClass cls) const;
		/** Per-role source counts for a group -- what the RIG group row reports. Roles absent from the
		 *  group's fixtures are omitted. Red/Green/Blue each report the colour-unit count; the GUI
		 *  collapses them into a single "colour" chip. */
		std::vector<std::pair<lx::EChannelRole, int>> sourceCountsFor(const lx::FixtureGroup& group) const;

		// --- Patches ---
		lx::Patch* createPatch(const std::string& name);
		lx::PatchParameter* addPatchParameter(lx::Patch& patch, rtti::TypeInfo type);
		void removePatchParameter(lx::Patch& patch, lx::PatchParameter* param);
		/** Creates a modulator with NO target. Deliberately: auto-targeting the first source meant every new
		 *  modulator arrived wired to something the user usually had to unwire first. */
		lx::Modulator* addModulator(lx::Patch& patch, rtti::TypeInfo type);
		void removeModulator(lx::Patch& patch, lx::Modulator* mod);
		void removePatch(lx::Patch* patch);
		/** Deep-copies a patch (all PatchParameters + Modulators, each modulator's runtime graph rebuilt as
		 *  on load), so a shared patch can be forked into an independent one. Returns the new patch. */
		lx::Patch* duplicatePatch(lx::Patch& src);
		const std::vector<rtti::ObjectPtr<lx::Patch>>& getPatches() const { return mPatches; }

		/**
		 * Authors a float curve track at runtime from a keyframe list, via the editor's SequenceControllerCurve.
		 * Clears any existing (auto-created) segments, lays one contiguous segment per interval [t_i, t_{i+1}],
		 * sets values (0..1) + per-point interpolation, and sets the sequence duration to the last key time.
		 * Main-thread only (StandardClock keeps authoring and playback from racing). This is the shared engine
		 * used by every modulator shape's generateCurve().
		 */
		void authorFloatCurve(SequenceEditor& editor, const std::string& trackID, const std::vector<lx::Key>& keys);

		// --- Triggers ---
		lx::Trigger* createTrigger(lx::ETriggerKind kind, const std::string& name);
		void setTriggerBindings(lx::Trigger& trigger, const std::vector<lx::PatchFixtureBinding>& bindings);
		void removeTrigger(lx::Trigger* trigger);
		const std::vector<rtti::ObjectPtr<lx::Trigger>>& getTriggers() const { return mTriggers; }
		uint64_t fireTrigger(lx::Trigger& trigger, bool held = false);
		void stopTrigger(lx::Trigger& trigger);
		bool isTriggerActive(lx::Trigger& trigger) const;
		/** Panic / All Stop: immediately stop every patch and drop every channel claim so output goes
		 *  dark this frame (no release-linger). A subsequent fireTrigger starts clean. */
		void stopAll();
		/** @return number of currently-held (non-releasing) activations -- the live "voices" count the GUI
		 *  shows so "Output live / N held" is honest rather than a hardcoded label. */
		size_t activeVoiceCount() const;

		// --- Controls + MIDI bindings ---
		lx::Control* createControl(const std::string& name, lx::EControlMode mode);
		lx::MidiBinding* createBinding(const MidiEvent& learnedEvent, lx::Control& control);
		void removeControl(lx::Control* control);
		void removeBinding(lx::MidiBinding* binding);
		const std::vector<rtti::ObjectPtr<lx::Control>>& getControls() const { return mControls; }
		const std::vector<rtti::ObjectPtr<lx::MidiBinding>>& getBindings() const { return mBindings; }

		// --- Programs ---
		lx::Program* createProgram(const std::string& name);
		void setProgramLifecycleTriggers(lx::Program& program, const std::vector<rtti::ObjectPtr<lx::Trigger>>& triggers);
		void removeProgram(lx::Program* program);
		void loadProgram(lx::Program* program);
		void unloadProgram();
		lx::Program* getActiveProgram() const { return mActiveProgram; }
		const std::vector<rtti::ObjectPtr<lx::Program>>& getPrograms() const { return mPrograms; }

		// --- Control mappings (per-Program: which Trigger a Control fires) ---
		lx::ControlMapping* setControlMapping(lx::Program& program, lx::Control& control, lx::Trigger* trigger);
		void clearControlMapping(lx::Program& program, lx::Control& control);
		lx::Trigger* getControlMapping(const lx::Program& program, const lx::Control& control) const;

		// --- Routing (control-first: 1 mapping <-> 1 dedicated Control-kind trigger <-> 1 binding).
		// The GUI owns the trigger lifecycle so the user never sees/names a Trigger. ---
		lx::ControlMapping* routeControl(lx::Program& program, lx::Control& control, lx::Patch* patch,
			const std::vector<nap::ResourcePtr<lx::FixtureGroup>>& groups);
		void setRoutingPatch(lx::Trigger& trigger, lx::Patch* patch);				///< rewrites the binding's patch (keeps its groups + spread)
		void setRoutingGroups(lx::Trigger& trigger, const std::vector<nap::ResourcePtr<lx::FixtureGroup>>& groups);	///< rewrites the binding's groups (keeps its patch + spread)
		void setRoutingSpread(lx::Trigger& trigger, bool endToEnd);				///< per-group (false) vs end-to-end (true); only meaningful with 2+ groups
		void unroute(lx::Program& program, lx::ControlMapping* mapping);			///< removes the routing + its dedicated trigger
		// Lifecycle routing (On load / On exit): one Enter/Exit trigger per program per kind.
		lx::Trigger* getLifecycleTrigger(const lx::Program& program, lx::ETriggerKind kind) const;
		lx::Trigger* ensureLifecycleTrigger(lx::Program& program, lx::ETriggerKind kind);
		void clearLifecycle(lx::Program& program, lx::ETriggerKind kind);
		// Output mix: distinct patch names currently claiming this fixture, winner (held, then newest) first.
		std::vector<std::string> fixtureClaimants(const lx::FixtureComponentInstance& fx) const;

		// --- MIDI log / learn ---
		const std::deque<std::string>& getMidiLog() const { return mMidiLog; }
		bool hasLastMidiEvent() const { return mHasLastEvent; }
		MidiEvent getLastMidiEvent() const { return MidiEvent(mLastEventType, mLastEventNumber, mLastEventValue, mLastEventChannel, mLastEventPort); }
		int getMidiEventCounter() const { return mMidiEventCounter; }

		/** Marks authored content dirty; update() flushes it to user_content.json on a ~0.5s debounce.
		 *  Every authoring mutation (incl. direct parameter/shape field edits made in the GUI) routes
		 *  through here, so persistence is uniform and no edit is silently lost. */
		void markDirty();

	private:
		struct ModulatorEntry
		{
			rtti::ObjectPtr<lx::Modulator>				mModulator;
			rtti::ObjectPtr<SequencePlayer>				mPlayer;
			rtti::ObjectPtr<SequencePlayerClock>			mClock;
			rtti::ObjectPtr<SequencePlayerCurveOutput>	mOutput;	// stock curve output -> sink parameter
			rtti::ObjectPtr<ParameterFloat>				mSink;
			rtti::ObjectPtr<SequenceEditor>				mEditor;	// runtime curve authoring + duration
			// A Field modulator's modulatable inputs (Rate/Density/...). Owned here so save() lists them as
			// root objects -- they are Default-pointer targets, which serializeObjects will not pull in.
			std::vector<rtti::ObjectPtr<lx::PatchParameter>>	mInputs;
		};

		struct PatchEntry
		{
			rtti::ObjectPtr<lx::Patch>						mPatch;
			std::vector<ModulatorEntry>						mModulators;
			std::vector<rtti::ObjectPtr<lx::PatchParameter>>	mParams;
			bool											mRemoved = false;
		};

		struct Activation
		{
			uint64_t					mId = 0;
			lx::Trigger*				mTrigger = nullptr;
			std::vector<lx::Patch*>	mPatches;
			bool						mReleasing = false;
			bool						mHeld = false;	// fired by a currently-held control (Momentary/Latch), not a stab/lifecycle
		};

		void onMidiEvent(const MidiEvent& event);
		/** (Re)builds one modulator's runtime player/sink/editor graph and re-propagates its voice count +
		 *  Noise seed bookkeeping. Called for each modulator by loadUserContent. */
		void rewireModulator(lx::Patch& patch, ModulatorEntry& entry);
		/** Creates any missing modulatable input parameter on a Field modulator and records them in `entry`
		 *  so they persist. `defaults` gives each input's authored 0..1 starting value, in inputs() order. */
		void ensureFieldInputs(lx::Modulator& mod, ModulatorEntry& entry);
		/** Deserializes user_content.json ourselves (never handed to the ResourceManager, so it is never
		 *  file-watched / hot-reloaded), resolves links, inits every resource references-first, then builds
		 *  the runtime modulator graphs + typed views. Returns false (state left empty) on a bad/old file. */
		bool loadUserContent(utility::ErrorState& errorState);
		/** Restores the program loaded last session (tiny sidecar file) and fires its Enter triggers. */
		void restoreActiveProgram();
		/** Writes just the active-program id sidecar -- cheap, separate from the full-content save(). */
		void saveSession();
		void save();
		std::string makeUniqueID(const std::string& base) const;
		PatchEntry* findEntry(lx::Patch& patch);
		bool buildModulatorGraph(ModulatorEntry& entry, const std::string& base, utility::ErrorState& errorState);
		lx::FixtureComponentInstance* findFixture(const std::string& entityID) const;
		void reapClaims(uint64_t activationId);
		// Shared erase: drops any ControlMapping matching the given predicate from both the service's
		// flat cache and every Program's mControlMappings list. Used by clearControlMapping,
		// removeControl, and removeTrigger.
		void eraseControlMappingsIf(const std::function<bool(const lx::ControlMapping&)>& pred);

		ResourceManager*					mResourceManager = nullptr;
		mutable std::unordered_set<std::string>	mIssuedIDs;	// every id makeUniqueID has handed out (createObject renames don't re-index the ResourceManager)
		rtti::OwnedObjectList				mOwnedContent;	// authored objects deserialized from user_content.json -- WE own them (not the ResourceManager), so the file is never watched/hot-reloaded
		bool								mDirty = false;		// authored content changed; update() flushes on a debounce
		double								mSaveTimer = 0.0;
		std::vector<lx::FixtureComponentInstance*>	mFixtures;
		ResourcePtr<MidiInputPort>			mMidiPort;
		std::unique_ptr<MidiHotplugMonitor>	mMidiHotplugMonitor;

		std::vector<rtti::ObjectPtr<lx::FixtureGroup>>	mGroups;

		std::vector<PatchEntry>				mPatchEntries;
		std::vector<rtti::ObjectPtr<lx::Patch>>	mPatches;	// mirrors mPatchEntries for getPatches()

		std::vector<rtti::ObjectPtr<lx::Trigger>>	mTriggers;
		std::vector<rtti::ObjectPtr<lx::Control>>	mControls;
		std::vector<rtti::ObjectPtr<lx::MidiBinding>>	mBindings;
		std::vector<rtti::ObjectPtr<lx::Program>>	mPrograms;
		std::vector<rtti::ObjectPtr<lx::ControlMapping>>	mControlMappings;
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
