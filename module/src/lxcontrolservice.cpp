// Local Includes
#include "lxcontrolservice.h"
#include "fixturecomponent.h"
#include "fixturechannelcomponent.h"
#include "noisemodulator.h"
#include "chasemodulator.h"
#include "gradientmodulator.h"

// External Includes
#include <nap/core.h>
#include <nap/resourcemanager.h>
#include <nap/logger.h>
#include <rtti/factory.h>
#include <rtti/jsonreader.h>
#include <rtti/defaultlinkresolver.h>
#include <rtti/object.h>
#include <rtti/jsonwriter.h>
#include <rtti/writer.h>
#include <rtti/rttiutilities.h>
#include <unordered_map>
#include <sequenceservice.h>
#include <sequence.h>
#include <sequencecontrollercurve.h>
#include <sequenceplayercurveoutput.h>
#include <utility/fileutils.h>
#include <cctype>
#include <algorithm>
#include <fstream>

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::lxcontrolService)
	RTTI_CONSTRUCTOR(nap::ServiceConfiguration*)
RTTI_END_CLASS

namespace nap
{
	static constexpr const char* sUserContentFile = "user_content.json";
	static constexpr const char* sSessionFile = "user_content.session";	// which program is loaded (runtime state)


	void lxcontrolService::getDependentServices(std::vector<rtti::TypeInfo>& dependencies)
	{
		dependencies.emplace_back(RTTI_OF(SequenceService));
	}


	void lxcontrolService::registerObjectCreators(rtti::Factory& factory)
	{
		// Modulators now use the stock SequencePlayerCurveOutput (registered by napsequence itself),
		// so lxcontrol no longer contributes any custom object creators.
	}


	bool lxcontrolService::init(nap::utility::ErrorState& errorState)
	{
		mResourceManager = getCore().getResourceManager();
		return true;
	}


	void lxcontrolService::update(double deltaTime)
	{
		// Debounced persistence: authoring edits mark content dirty (and reset the timer); flush the write
		// at most ~2x/second. Because we own user_content.json (it's never handed to the ResourceManager),
		// this write can no longer trip a hot-reload -- so it's safe to do while patches are animating.
		if (mDirty)
		{
			mSaveTimer += deltaTime;
			if (mSaveTimer >= 0.5)
			{
				save();
				mDirty = false;
				mSaveTimer = 0.0;
			}
		}

		// Drive every patch so its modulators (fed by their player clocks / adapters) blend into the
		// patch parameters each frame.
		for (auto& entry : mPatchEntries)
		{
			if (!entry.mRemoved && entry.mPatch != nullptr)
				entry.mPatch->update(deltaTime);
		}

		// Reap releasing activations whose patches have all finished (release-linger): their claims
		// stay until then, so a stopped ADSR rings out before the channel reverts.
		for (auto it = mActivations.begin(); it != mActivations.end(); )
		{
			bool finished = it->mReleasing;
			for (auto* e : it->mPatches)
			{
				if (finished && !e->isFinished())
					finished = false;
			}
			if (finished)
			{
				reapClaims(it->mId);
				it = mActivations.erase(it);
			}
			else
			{
				++it;
			}
		}

		// MIDI hot-plug reconnect
		if (mMidiPort == nullptr || mMidiHotplugMonitor == nullptr)
			return;

		std::vector<std::string> new_ports;
		if (!mMidiHotplugMonitor->update(deltaTime, new_ports))
			return;

		mMidiPort->stop();
		utility::ErrorState error;
		if (mMidiPort->start(error))
			mMidiLog.push_front("MIDI devices changed - reconnected: " + mMidiPort->getPortNames());
		else
			mMidiLog.push_front("MIDI devices changed - reconnect failed: " + error.toString());
		while (mMidiLog.size() > sMaxMidiLogSize)
			mMidiLog.pop_back();
	}


	void lxcontrolService::shutdown()
	{
		if (mDirty)
			save();
	}


	bool lxcontrolService::setup(MidiInputComponentInstance& midiSource, ResourcePtr<MidiInputPort> midiPort, utility::ErrorState& errorState)
	{
		mMidiPort = midiPort;
		mMidiHotplugMonitor = std::make_unique<MidiHotplugMonitor>();
		midiSource.messageReceived.connect(mMidiSlot);

		// We deserialize user_content.json ourselves and own the objects, so the ResourceManager never
		// file-watches it. Writing it back (save()) therefore can no longer trip a hot-reload -- which used
		// to reset a live-animating modulator mid-show and silently drop un-saved field edits. No
		// reload-rewire machinery is needed any more.
		if (utility::fileExists(sUserContentFile))
		{
			utility::ErrorState loadError;
			if (!loadUserContent(loadError))
			{
				// Unloadable (e.g. old-format) content must never halt startup: rename and continue empty.
				std::string backup = std::string(sUserContentFile) + ".bak";
				std::remove(backup.c_str());
				std::rename(sUserContentFile, backup.c_str());
				Logger::warn("lxcontrolService: could not load %s (%s); renamed to %s, continuing empty",
					sUserContentFile, loadError.toString().c_str(), backup.c_str());
			}
			else
			{
				restoreActiveProgram();
			}
		}

		// A fresh install must have something bindable. Runs after the load so it matches a group that was
		// already persisted instead of creating a duplicate.
		ensureDefaultGroup();
		return true;
	}


	void lxcontrolService::markDirty()
	{
		mDirty = true;
		mSaveTimer = 0.0;	// debounce: flush ~0.5s after the LAST edit
	}


	void lxcontrolService::saveSession()
	{
		std::ofstream sess(sSessionFile, std::ios::trunc);
		if (mActiveProgram != nullptr)
			sess << mActiveProgram->mID;
	}


	bool lxcontrolService::loadUserContent(utility::ErrorState& errorState)
	{
		rtti::DeserializeResult result;
		if (!rtti::deserializeJSONFile(sUserContentFile, rtti::EPropertyValidationMode::AllowMissingProperties,
				rtti::EPointerPropertyMode::NoRawPointers, mResourceManager->getFactory(), result, errorState))
			return false;
		if (!rtti::DefaultLinkResolver::sResolveLinks(result.mReadObjects, result.mUnresolvedPointers, errorState))
			return false;

		// Init every resource references-first (params -> modulators -> patches -> triggers -> ...), the
		// order the old ResourceManager loadFile path used before rebuildFromLoadedContent ran.
		std::unordered_set<rtti::Object*> inited;
		auto initType = [&](const rtti::TypeInfo& base) -> bool {
			for (auto& o : result.mReadObjects)
			{
				if (inited.count(o.get()) || !(o->get_type() == base || o->get_type().is_derived_from(base)))
					continue;
				if (auto* r = rtti_cast<Resource>(o.get()))
					if (!r->init(errorState))
						return false;
				inited.insert(o.get());
			}
			return true;
		};
		if (!initType(RTTI_OF(lx::PatchParameter)) || !initType(RTTI_OF(lx::Modulator)) ||
			!initType(RTTI_OF(lx::Patch)) || !initType(RTTI_OF(lx::Control)) ||
			!initType(RTTI_OF(lx::MidiBinding)) || !initType(RTTI_OF(lx::Trigger)) ||
			!initType(RTTI_OF(lx::ControlMapping)) || !initType(RTTI_OF(lx::Program)) ||
			!initType(RTTI_OF(Resource)))
			return false;

		// Take ownership + register loaded ids so a later runtime createObject() can't collide with one.
		for (auto& o : result.mReadObjects)
		{
			mIssuedIDs.insert(o->mID);
			mOwnedContent.emplace_back(std::move(o));
		}

		// Build the typed views + runtime modulator graphs from the objects we now own (this replaces the
		// old rebuildFromLoadedContent, which scanned mResourceManager->getObjects<T>() instead).
		for (auto& o : mOwnedContent)
		{
			rtti::Object* obj = o.get();
			if (auto* patch = rtti_cast<lx::Patch>(obj))
			{
				PatchEntry entry;
				entry.mPatch = rtti::ObjectPtr<lx::Patch>(patch);
				for (auto& p : patch->mParameters)
					entry.mParams.emplace_back(rtti::ObjectPtr<lx::PatchParameter>(p.get()));
				for (auto& m : patch->mModulators)
				{
					ModulatorEntry me;
					me.mModulator = rtti::ObjectPtr<lx::Modulator>(m.get());
					rewireModulator(*patch, me);
					entry.mModulators.emplace_back(me);
				}
				mPatchEntries.emplace_back(std::move(entry));
				mPatches.emplace_back(rtti::ObjectPtr<lx::Patch>(patch));
			}
			else if (auto* trigger = rtti_cast<lx::Trigger>(obj))
				mTriggers.emplace_back(rtti::ObjectPtr<lx::Trigger>(trigger));
			else if (auto* control = rtti_cast<lx::Control>(obj))
				mControls.emplace_back(rtti::ObjectPtr<lx::Control>(control));
			else if (auto* binding = rtti_cast<lx::MidiBinding>(obj))
				mBindings.emplace_back(rtti::ObjectPtr<lx::MidiBinding>(binding));
			else if (auto* program = rtti_cast<lx::Program>(obj))
				mPrograms.emplace_back(rtti::ObjectPtr<lx::Program>(program));
			else if (auto* mapping = rtti_cast<lx::ControlMapping>(obj))
				mControlMappings.emplace_back(rtti::ObjectPtr<lx::ControlMapping>(mapping));
			else if (auto* group = rtti_cast<lx::FixtureGroup>(obj))
				mGroups.emplace_back(rtti::ObjectPtr<lx::FixtureGroup>(group));
		}
		return true;
	}


	void lxcontrolService::restoreActiveProgram()
	{
		std::ifstream sess(sSessionFile);
		if (!sess.is_open())
			return;
		std::string id;
		std::getline(sess, id);
		if (id.empty())
			return;
		for (auto& p : mPrograms)
			if (p != nullptr && p->mID == id)
			{
				loadProgram(p.get());
				return;
			}
	}


	void lxcontrolService::rewireModulator(lx::Patch& patch, ModulatorEntry& entry)
	{
		lx::Modulator* mod = entry.mModulator.get();
		if (mod == nullptr)
			return;

		utility::ErrorState err;
		if (!buildModulatorGraph(entry, makeUniqueID(mod->mID + "_rt"), err))
		{
			Logger::error("rewireModulator: build graph failed for %s: %s", mod->mID.c_str(), err.toString().c_str());
			return;
		}

		// Re-register (or back-fill) this Field modulator's input parameters so they persist again and any
		// input added since the file was written gets created rather than staying null.
		ensureFieldInputs(*mod, entry);
	}


	void lxcontrolService::ensureFieldInputs(lx::Modulator& mod, ModulatorEntry& entry)
	{
		auto* field = rtti_cast<lx::FieldModulator>(&mod);
		if (field == nullptr)
			return;

		// Wire each declared-but-null input slot. Role Generic on purpose: no rig channel maps Generic, so
		// an input can never be claimed by a fixture even if it leaked into a patch's parameter list.
		auto make = [&](nap::ResourcePtr<lx::FloatParameter>& slot, const char* name, float base)
		{
			if (slot != nullptr)
			{
				entry.mInputs.emplace_back(rtti::ObjectPtr<lx::PatchParameter>(slot.get()));
				return;
			}
			auto p = mResourceManager->createObject<lx::FloatParameter>();
			p->mID = makeUniqueID(mod.mID + "_" + name);
			p->mName = name;
			p->mRole = lx::EChannelRole::Generic;
			p->mValue = base;
			utility::ErrorState err;
			if (!p->init(err))
			{
				Logger::error("ensureFieldInputs: %s", err.toString().c_str());
				return;
			}
			slot = p.get();
			entry.mInputs.emplace_back(rtti::ObjectPtr<lx::PatchParameter>(p.get()));
		};

		// Colour inputs (Gradient's endpoints) need their own maker: a ColorParameter, not a float.
		auto makeColor = [&](nap::ResourcePtr<lx::ColorParameter>& slot, const char* name, float r, float g, float b)
		{
			if (slot != nullptr)
			{
				entry.mInputs.emplace_back(rtti::ObjectPtr<lx::PatchParameter>(slot.get()));
				return;
			}
			auto p = mResourceManager->createObject<lx::ColorParameter>();
			p->mID = makeUniqueID(mod.mID + "_" + name);
			p->mName = name;
			p->mRed = r; p->mGreen = g; p->mBlue = b;
			utility::ErrorState err;
			if (!p->init(err))
			{
				Logger::error("ensureFieldInputs: %s", err.toString().c_str());
				return;
			}
			slot = p.get();
			entry.mInputs.emplace_back(rtti::ObjectPtr<lx::PatchParameter>(p.get()));
		};

		if (auto* chase = rtti_cast<lx::ChaseModulator>(field))
		{
			make(chase->mRateInput, "Rate", 0.12f);			// ~1 Hz within [0.05, 8]
			make(chase->mPulseWidthInput, "PulseWidth", 0.3f);
		}
		else if (auto* grad = rtti_cast<lx::GradientModulator>(field))
		{
			makeColor(grad->mStartInput, "Start", 0.0f, 0.22f, 1.0f);	// blue
			makeColor(grad->mEndInput, "End", 0.96f, 0.70f, 0.0f);		// gold
			make(grad->mPhaseInput, "Phase", 0.0f);
			make(grad->mPeriodInput, "Period", 0.23f);					// ~1 span within [0.1, 4]
		}
		else if (auto* noise = rtti_cast<lx::NoiseModulator>(field))
		{
			make(noise->mRateInput, "Rate", 0.25f);			// ~2 Hz within [0.05, 8]
			make(noise->mDensityInput, "Density", 0.35f);	// ~9 cells within [1, 24]
			make(noise->mSmoothingInput, "Smoothing", 0.5f);
		}
	}


	void lxcontrolService::registerFixture(lx::FixtureComponentInstance* fixture)
	{
		if (std::find(mFixtures.begin(), mFixtures.end(), fixture) == mFixtures.end())
			mFixtures.emplace_back(fixture);
	}


	void lxcontrolService::unregisterFixture(lx::FixtureComponentInstance* fixture)
	{
		mFixtures.erase(std::remove(mFixtures.begin(), mFixtures.end(), fixture), mFixtures.end());
	}


	std::vector<lx::FixtureComponentInstance*> lxcontrolService::getFixturesPhysicalOrder() const
	{
		std::vector<lx::FixtureComponentInstance*> ordered = mFixtures;
		std::sort(ordered.begin(), ordered.end(),
			[](lx::FixtureComponentInstance* a, lx::FixtureComponentInstance* b) { return a->getStartChannel() < b->getStartChannel(); });
		return ordered;
	}


	std::string lxcontrolService::makeUniqueID(const std::string& base) const
	{
		// Check both the ResourceManager AND our own issued-id set: createObject registers an object under
		// its original id, but we reassign mID afterwards, so findObject can't see runtime-renamed objects
		// (which is how two modulators on one patch used to collide and break user_content.json reload).
		auto taken = [this](const std::string& id)
		{
			return mResourceManager->findObject(id) != nullptr || mIssuedIDs.count(id) > 0;
		};

		std::string result = base;
		if (taken(result))
		{
			int suffix = 2;
			while (taken(base + "_" + std::to_string(suffix)))
				suffix++;
			result = base + "_" + std::to_string(suffix);
		}
		mIssuedIDs.insert(result);
		return result;
	}


	lxcontrolService::PatchEntry* lxcontrolService::findEntry(lx::Patch& patch)
	{
		for (auto& entry : mPatchEntries)
		{
			if (entry.mPatch.get() == &patch)
				return &entry;
		}
		return nullptr;
	}


	lx::FixtureGroup* lxcontrolService::createGroup(const std::string& name)
	{
		auto group = mResourceManager->createObject<lx::FixtureGroup>();
		group->mID = makeUniqueID("Group_" + name);
		group->mName = name;
		utility::ErrorState err;
		if (!group->init(err))
		{
			Logger::error("createGroup: %s", err.toString().c_str());
			return nullptr;
		}
		mGroups.emplace_back(group);
		markDirty();
		return group.get();
	}


	void lxcontrolService::removeGroup(lx::FixtureGroup* group)
	{
		mGroups.erase(std::remove_if(mGroups.begin(), mGroups.end(),
			[group](const rtti::ObjectPtr<lx::FixtureGroup>& g) { return g.get() == group; }), mGroups.end());
		markDirty();
	}


	void lxcontrolService::setGroupFixtures(lx::FixtureGroup& group, const std::vector<std::string>& fixtureIDs)
	{
		group.mFixtureNames = fixtureIDs;
		markDirty();
	}


	lx::FixtureGroup* lxcontrolService::ensureDefaultGroup()
	{
		for (auto& g : mGroups)
		{
			if (g != nullptr && g->mName == "All Fixtures")
				return g.get();
		}

		lx::FixtureGroup* group = createGroup("All Fixtures");
		if (group == nullptr)
			return nullptr;

		std::vector<std::string> ids;
		for (auto* f : getFixturesPhysicalOrder())
			ids.emplace_back(f->getEntityID());
		setGroupFixtures(*group, ids);
		return group;
	}


	std::vector<lx::Source> lxcontrolService::sourcesFor(const lx::FixtureGroup& group, lx::ESpreadClass cls) const
	{
		std::vector<lx::Source> sources;
		for (const std::string& id : group.mFixtureNames)
		{
			lx::FixtureComponentInstance* fixture = findFixture(id);
			if (fixture == nullptr)
				continue;		// group outlived a fixture: skip it rather than shifting every position

			if (cls == lx::ESpreadClass::Fixture)
			{
				sources.push_back({ fixture, 0 });
				continue;
			}

			// Colour units, ascending, deduplicated: the three R/G/B channels share one unit index.
			std::vector<int> units;
			for (auto* channel : fixture->getChannels())
			{
				if (lx::spreadClassOf(channel->getRole()) != lx::ESpreadClass::ColourUnit)
					continue;
				if (std::find(units.begin(), units.end(), channel->getUnitIndex()) == units.end())
					units.emplace_back(channel->getUnitIndex());
			}
			std::sort(units.begin(), units.end());
			for (int u : units)
				sources.push_back({ fixture, u });
		}
		return sources;
	}


	std::vector<std::pair<lx::EChannelRole, int>> lxcontrolService::sourceCountsFor(const lx::FixtureGroup& group) const
	{
		const int fixture_sources = static_cast<int>(sourcesFor(group, lx::ESpreadClass::Fixture).size());
		const int colour_sources = static_cast<int>(sourcesFor(group, lx::ESpreadClass::ColourUnit).size());

		// Only report roles the group's fixtures actually have.
		std::vector<std::pair<lx::EChannelRole, int>> counts;
		std::vector<lx::EChannelRole> seen;
		for (const std::string& id : group.mFixtureNames)
		{
			lx::FixtureComponentInstance* fixture = findFixture(id);
			if (fixture == nullptr)
				continue;
			for (auto* channel : fixture->getChannels())
			{
				const lx::EChannelRole role = channel->getRole();
				if (std::find(seen.begin(), seen.end(), role) != seen.end())
					continue;
				seen.emplace_back(role);
				const bool colour = lx::spreadClassOf(role) == lx::ESpreadClass::ColourUnit;
				counts.emplace_back(role, colour ? colour_sources : fixture_sources);
			}
		}
		return counts;
	}


	lx::Patch* lxcontrolService::createPatch(const std::string& name)
	{
		auto patch = mResourceManager->createObject<lx::Patch>();
		patch->mID = makeUniqueID("Patch_" + name);
		patch->mName = name;

		utility::ErrorState err;
		if (!patch->init(err))
		{
			Logger::error("createPatch: %s", err.toString().c_str());
			return nullptr;
		}

		PatchEntry entry;
		entry.mPatch = patch;
		mPatchEntries.emplace_back(entry);
		mPatches.emplace_back(patch);
		markDirty();
		return patch.get();
	}


	lx::PatchParameter* lxcontrolService::addPatchParameter(lx::Patch& patch, rtti::TypeInfo type)
	{
		PatchEntry* entry = findEntry(patch);
		if (entry == nullptr)
			return nullptr;

		auto obj = mResourceManager->createObject(type);
		auto* param = rtti_cast<lx::PatchParameter>(obj.get());
		if (param == nullptr)
		{
			Logger::error("addPatchParameter: type %s is not an PatchParameter", type.get_name().data());
			return nullptr;
		}

		param->mID = makeUniqueID(patch.mID + "_Param");
		param->mName = "Param";
		utility::ErrorState err;
		if (!param->init(err))
		{
			Logger::error("addPatchParameter: %s", err.toString().c_str());
			return nullptr;
		}

		patch.mParameters.emplace_back(param);
		entry->mParams.emplace_back(rtti::ObjectPtr<lx::PatchParameter>(param));
		markDirty();
		return param;
	}


	bool lxcontrolService::buildModulatorGraph(ModulatorEntry& entry, const std::string& base, utility::ErrorState& err)
	{
		lx::Modulator* mod = entry.mModulator.get();

		// StandardClock: modulator playback + curve (re)authoring both run on the main thread, so no locking.
		auto clock = mResourceManager->createObject<SequencePlayerStandardClock>();
		clock->mID = makeUniqueID(base + "_Clock");
		if (!clock->init(err))
			return false;
		entry.mClock = clock;

		// Sink parameter the stock curve adapter writes into; Modulator::value() reads it back.
		auto sink = mResourceManager->createObject<ParameterFloat>();
		sink->mID = makeUniqueID(base + "_Sink");
		sink->mMinimum = 0.0f; sink->mMaximum = 1.0f; sink->mValue = 0.0f;
		if (!sink->init(err))
			return false;
		entry.mSink = sink;

		auto output = mResourceManager->createObject<SequencePlayerCurveOutput>();
		output->mID = makeUniqueID(base + "_Output");
		output->mParameter = sink;
		output->mUseMainThread = true;
		if (!output->init(err))
			return false;
		entry.mOutput = output;

		auto player = mResourceManager->createObject<SequencePlayer>();
		player->mID = makeUniqueID(base + "_Player");
		player->mSequenceFileName = base + "_seq.json";	// non-existent -> empty sequence + auto curve track
		player->mCreateEmptySequenceOnLoadFail = true;
		player->mClock = entry.mClock;
		player->mOutputs.emplace_back(rtti::ObjectPtr<SequencePlayerOutput>(output.get()));
		if (!player->init(err))
			return false;
		if (!player->start(err))
			return false;
		entry.mPlayer = player;

		auto editor = mResourceManager->createObject<SequenceEditor>();
		editor->mID = makeUniqueID(base + "_Editor");
		editor->mSequencePlayer = player;
		if (!editor->init(err))
			return false;
		entry.mEditor = editor;

		// napsequence auto-creates one curve track bound to the curve output on empty-sequence load;
		// reuse it, else create one explicitly.
		std::string track_id;
		auto& ctrl = editor->getController<SequenceControllerCurve>();
		if (!player->getSequenceConst().mTracks.empty())
		{
			track_id = player->getSequenceConst().mTracks.back()->mID;
		}
		else
		{
			ctrl.addNewCurveTrack<float>();
			if (player->getSequenceConst().mTracks.empty())
			{
				err.fail("buildModulatorGraph: could not create a curve track");
				return false;
			}
			track_id = player->getSequenceConst().mTracks.back()->mID;
			ctrl.assignNewOutputID(track_id, output->mID);
		}
		ctrl.changeMinMaxCurveTrack<float>(track_id, 0.0f, 1.0f);

		// Wire runtime handles, then let the shape author its curve (sets mDuration + sequence duration).
		mod->mPlayer = player.get();
		mod->mSink = sink.get();
		mod->mEditor = editor.get();
		mod->mTrackID = track_id;
		mod->generateCurve(*this);

		// Built idle: value holds at the curve's start until a trigger drives the transport.
		player->setPlayerTime(0.0);
		player->setIsPlaying(false);
		return true;
	}


	lx::Modulator* lxcontrolService::addModulator(lx::Patch& patch, rtti::TypeInfo type)
	{
		PatchEntry* patch_entry = findEntry(patch);
		if (patch_entry == nullptr)
			return nullptr;

		auto obj = mResourceManager->createObject(type);
		auto* mod = rtti_cast<lx::Modulator>(obj.get());
		if (mod == nullptr)
		{
			Logger::error("addModulator: type %s is not a Modulator", type.get_name().data());
			return nullptr;
		}

		mod->mID = makeUniqueID(patch.mID + "_Mod");
		// Short, readable default name: "lx::LfoModulator" -> "LFO". The raw RTTI name leaked into the
		// blend-chain readout and the "driven by X" markers, where it was unreadable.
		{
			std::string n = std::string(type.get_name().data());
			const size_t colons = n.rfind("::");
			if (colons != std::string::npos)
				n = n.substr(colons + 2);
			const std::string suffix = "Modulator";
			if (n.size() > suffix.size() && n.compare(n.size() - suffix.size(), suffix.size(), suffix) == 0)
				n = n.substr(0, n.size() - suffix.size());
			for (char& c : n)
				c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
			mod->mName = n;
		}

		utility::ErrorState err;
		if (!mod->init(err))
		{
			Logger::error("addModulator: %s", err.toString().c_str());
			return nullptr;
		}

		ModulatorEntry mod_entry;
		mod_entry.mModulator = rtti::ObjectPtr<lx::Modulator>(mod);
		if (!buildModulatorGraph(mod_entry, mod->mID, err))
		{
			Logger::error("addModulator: build graph failed: %s", err.toString().c_str());
			return nullptr;
		}
		ensureFieldInputs(*mod, mod_entry);
		patch.mModulators.emplace_back(mod);
		patch_entry->mModulators.emplace_back(mod_entry);
		markDirty();
		return mod;
	}


	void lxcontrolService::removeModulator(lx::Patch& patch, lx::Modulator* mod)
	{
		if (mod == nullptr)
			return;
		PatchEntry* entry = findEntry(patch);
		if (entry != nullptr)
		{
			for (auto& me : entry->mModulators)
				if (me.mModulator.get() == mod && me.mPlayer != nullptr)
					me.mPlayer->setIsPlaying(false);
			entry->mModulators.erase(std::remove_if(entry->mModulators.begin(), entry->mModulators.end(),
				[mod](const ModulatorEntry& me) { return me.mModulator.get() == mod; }), entry->mModulators.end());
		}
		patch.mModulators.erase(std::remove_if(patch.mModulators.begin(), patch.mModulators.end(),
			[mod](const nap::ResourcePtr<lx::Modulator>& m) { return m.get() == mod; }), patch.mModulators.end());
		markDirty();
	}


	void lxcontrolService::removePatchParameter(lx::Patch& patch, lx::PatchParameter* param)
	{
		if (param == nullptr)
			return;
		// Scrub the parameter from every modulator's target list so nothing points at a freed source.
		for (auto& m : patch.mModulators)
		{
			auto& tv = m->mTargets;
			tv.erase(std::remove_if(tv.begin(), tv.end(),
				[param](const nap::ResourcePtr<lx::PatchParameter>& t) { return t.get() == param; }), tv.end());
			if (m->mTarget.get() == param)
				m->mTarget = nullptr;
		}
		PatchEntry* entry = findEntry(patch);
		if (entry != nullptr)
			entry->mParams.erase(std::remove_if(entry->mParams.begin(), entry->mParams.end(),
				[param](const rtti::ObjectPtr<lx::PatchParameter>& p) { return p.get() == param; }), entry->mParams.end());
		patch.mParameters.erase(std::remove_if(patch.mParameters.begin(), patch.mParameters.end(),
			[param](const nap::ResourcePtr<lx::PatchParameter>& p) { return p.get() == param; }), patch.mParameters.end());
		markDirty();
	}




	void lxcontrolService::removePatch(lx::Patch* patch)
	{
		// Delete-path guard: reap any activation that references this patch so no stale claim pins a
		// channel to a frozen value.
		for (auto it = mActivations.begin(); it != mActivations.end(); )
		{
			bool references = std::find(it->mPatches.begin(), it->mPatches.end(), patch) != it->mPatches.end();
			if (references)
			{
				reapClaims(it->mId);
				it = mActivations.erase(it);
			}
			else
			{
				++it;
			}
		}

		for (auto& entry : mPatchEntries)
		{
			if (entry.mPatch.get() != patch)
				continue;
			for (auto& me : entry.mModulators)
			{
				if (me.mPlayer != nullptr)
					me.mPlayer->setIsPlaying(false);
			}
			entry.mRemoved = true;
			break;
		}
		mPatches.erase(std::remove_if(mPatches.begin(), mPatches.end(),
			[patch](const rtti::ObjectPtr<lx::Patch>& e) { return e.get() == patch; }), mPatches.end());
		markDirty();
	}


	lx::Patch* lxcontrolService::duplicatePatch(lx::Patch& src)
	{
		lx::Patch* dst = createPatch(src.mName + " copy");
		if (dst == nullptr)
			return nullptr;

		// Deep-copy parameters (same type, all rtti fields), keeping a source->dst map to remap modulator
		// targets. copyObject clobbers mID too, so we restore the unique id it just handed us.
		std::unordered_map<lx::PatchParameter*, lx::PatchParameter*> pmap;
		for (auto& sp : src.mParameters)
		{
			lx::PatchParameter* np = addPatchParameter(*dst, sp->get_type());
			if (np == nullptr)
				continue;
			const std::string id = np->mID;
			rtti::copyObject(*sp, *np);
			np->mID = id;
			pmap[sp.get()] = np;
		}

		// Deep-copy modulators: create same type, copy shape/blend fields, remap the copied target pointers
		// onto the dst params, then rebuild the runtime graph exactly as the load path does (rewireModulator).
		PatchEntry* dentry = findEntry(*dst);
		for (auto& sm : src.mModulators)
		{
			// No seed target needed: copyObject brings mTargets across and they are remapped just below.
			lx::Modulator* nm = addModulator(*dst, sm->get_type());
			if (nm == nullptr)
				continue;
			const std::string id = nm->mID;
			rtti::copyObject(*sm, *nm);	// shape fields + Min/Max/Blend + mTargets (still point at src params) + mID
			nm->mID = id;

			nm->mTarget = nullptr;
			std::vector<nap::ResourcePtr<lx::PatchParameter>> remapped;
			for (auto& t : nm->mTargets)
			{
				auto it = pmap.find(t.get());
				if (it != pmap.end()) remapped.emplace_back(it->second);
			}
			nm->mTargets = remapped;

			if (dentry != nullptr)
				for (auto& me : dentry->mModulators)
					if (me.mModulator.get() == nm) { rewireModulator(*dst, me); break; }
		}

		markDirty();
		return dst;
	}


	lx::FixtureComponentInstance* lxcontrolService::findFixture(const std::string& entityID) const
	{
		for (auto* fixture : mFixtures)
		{
			if (fixture->getEntityID() == entityID)
				return fixture;
		}
		return nullptr;
	}


	void lxcontrolService::reapClaims(uint64_t activationId)
	{
		for (auto* fixture : mFixtures)
		{
			for (auto* channel : fixture->getChannels())
				channel->removeClaims(activationId);
		}
	}


	lx::Trigger* lxcontrolService::createTrigger(lx::ETriggerKind kind, const std::string& name)
	{
		auto obj = mResourceManager->createObject(RTTI_OF(lx::Trigger));
		auto* trigger = rtti_cast<lx::Trigger>(obj.get());
		trigger->mID = makeUniqueID("Trigger_" + name);
		trigger->mName = name;
		trigger->mKind = kind;
		utility::ErrorState err;
		if (!trigger->init(err))
		{
			Logger::error("createTrigger: %s", err.toString().c_str());
			return nullptr;
		}
		mTriggers.emplace_back(trigger);
		markDirty();
		return trigger;
	}


	void lxcontrolService::setTriggerBindings(lx::Trigger& trigger, const std::vector<lx::PatchFixtureBinding>& bindings)
	{
		trigger.mBindings = bindings;
		markDirty();
	}


	void lxcontrolService::removeTrigger(lx::Trigger* trigger)
	{
		if (trigger != nullptr)
			stopTrigger(*trigger);
		mTriggers.erase(std::remove_if(mTriggers.begin(), mTriggers.end(),
			[trigger](const rtti::ObjectPtr<lx::Trigger>& t) { return t.get() == trigger; }), mTriggers.end());

		// Scrub any now-dangling ControlMapping referencing this trigger, and drop it from every
		// Program's lifecycle-trigger list too (a latent bug pre-dating this refactor: removeTrigger
		// never scrubbed Program::mTriggers either).
		eraseControlMappingsIf([trigger](const lx::ControlMapping& m) { return m.mTrigger.get() == trigger; });
		for (auto& program : mPrograms)
		{
			auto& list = program->mLifecycleTriggers;
			list.erase(std::remove_if(list.begin(), list.end(),
				[trigger](const nap::ResourcePtr<lx::Trigger>& t) { return t.get() == trigger; }), list.end());
		}
		markDirty();
	}


	uint64_t lxcontrolService::fireTrigger(lx::Trigger& trigger, bool held)
	{
		// Retrigger: stop-then-replace any existing activation of this trigger.
		stopTrigger(trigger);

		Activation activation;
		activation.mId = mNextActivationId++;
		activation.mTrigger = &trigger;
		activation.mHeld = held;

		for (auto& binding : trigger.mBindings)
		{
			lx::Patch* patch = binding.mPatch.get();
			if (patch == nullptr)
				continue;
			activation.mPatches.emplace_back(patch);

			// Spread each class over its own source list. A group of 3 six-unit fixtures yields 3 Fixture
			// sources (Dimmer/Strobe/...) and 18 ColourUnit sources, so one Chase steps 3 times on a dimmer
			// and 18 times across the colour units -- from the same authored shape, with nothing declared.
			// Source ORDER follows the group's own member order, which is why reordering a group (or Rev)
			// reverses the sweep.
			for (lx::ESpreadClass cls : { lx::ESpreadClass::Fixture, lx::ESpreadClass::ColourUnit })
			{
				// Per group (default): each group spans 0..1 on its own, so they run in parallel.
				// End-to-end: concatenated into one run, so the effect crosses the whole set once.
				std::vector<std::vector<lx::Source>> runs;
				if (binding.mEndToEnd)
				{
					std::vector<lx::Source> all;
					for (auto& g : binding.mGroups)
						if (g != nullptr)
							for (auto& s : sourcesFor(*g.get(), cls))
								all.emplace_back(s);
					runs.emplace_back(std::move(all));
				}
				else
				{
					for (auto& g : binding.mGroups)
						if (g != nullptr)
							runs.emplace_back(sourcesFor(*g.get(), cls));
				}

				for (auto& run : runs)
				{
					const int source_count = static_cast<int>(run.size());
					for (int idx = 0; idx < source_count; ++idx)
					{
						lx::Source& src = run[idx];
						for (auto* channel : src.mFixture->getChannels())
						{
							if (lx::spreadClassOf(channel->getRole()) != cls)
								continue;
							// A ColourUnit source addresses exactly one unit of its fixture.
							if (cls == lx::ESpreadClass::ColourUnit && channel->getUnitIndex() != src.mUnitIndex)
								continue;

							for (auto& param : patch->mParameters)
							{
								const int comps = param->getComponentCount();
								for (int c = 0; c < comps; ++c)
									if (param->getComponentRole(c) == channel->getRole())
										channel->pushClaim(activation.mId, patch, param.get(), c, idx, source_count, held);
							}
						}
					}
				}
			}
			patch->trigger();
		}

		mActivations.emplace_back(activation);
		return activation.mId;
	}


	void lxcontrolService::stopTrigger(lx::Trigger& trigger)
	{
		for (auto& activation : mActivations)
		{
			if (activation.mTrigger != &trigger || activation.mReleasing)
				continue;
			for (auto* patch : activation.mPatches)
				patch->stop();
			// This gesture is no longer held: release its claims so any still-held control reclaims the
			// channel; the released claim then shows only where nothing held remains (it rings out).
			for (auto* fixture : mFixtures)
				for (auto* channel : fixture->getChannels())
					channel->releaseClaims(activation.mId);
			activation.mReleasing = true;
			activation.mHeld = false;
		}
	}


	bool lxcontrolService::isTriggerActive(lx::Trigger& trigger) const
	{
		for (auto& activation : mActivations)
		{
			if (activation.mTrigger == &trigger)
				return true;
		}
		return false;
	}


	void lxcontrolService::stopAll()
	{
		// Panic: stop every patch, drop every claim, clear all activations -> output is dark this frame.
		for (auto& activation : mActivations)
		{
			for (auto* patch : activation.mPatches)
				patch->stop();
			reapClaims(activation.mId);
		}
		mActivations.clear();
	}


	size_t lxcontrolService::activeVoiceCount() const
	{
		size_t n = 0;
		for (auto& activation : mActivations)
			if (activation.mHeld && !activation.mReleasing)
				++n;
		return n;
	}


	lx::Control* lxcontrolService::createControl(const std::string& name, lx::EControlMode mode)
	{
		auto control = mResourceManager->createObject<lx::Control>();
		control->mID = makeUniqueID("Control_" + name);
		control->mName = name;
		control->mMode = mode;
		utility::ErrorState err;
		if (!control->init(err))
		{
			Logger::error("createControl: %s", err.toString().c_str());
			return nullptr;
		}
		mControls.emplace_back(control);
		markDirty();
		return control.get();
	}


	lx::MidiBinding* lxcontrolService::createBinding(const MidiEvent& learnedEvent, lx::Control& control)
	{
		auto binding = mResourceManager->createObject<lx::MidiBinding>();
		binding->mID = makeUniqueID("Binding_" + std::to_string(mBindings.size() + 1));

		// Device-agnostic by default: message type + number only (no port/channel).
		binding->mNumbers = { static_cast<int>(learnedEvent.getNumber()) };
		switch (learnedEvent.getType())
		{
		// A note binding matches both on and off so Momentary controls can release; Toggle/FireOnly
		// ignore off-events, so this is harmless for them.
		case MidiEvent::Type::noteOn:
		case MidiEvent::Type::noteOff:			binding->mNoteOn = true; binding->mNoteOff = true;	break;
		case MidiEvent::Type::afterTouch:		binding->mAftertouch = true;		break;
		case MidiEvent::Type::controlChange:	binding->mControlChange = true;		break;
		case MidiEvent::Type::programChange:	binding->mProgramChange = true;		break;
		case MidiEvent::Type::channelPressure:	binding->mChannelPressure = true;	break;
		case MidiEvent::Type::pitchBend:		binding->mPitchBend = true;			break;
		default:								break;
		}
		binding->mControl = &control;

		utility::ErrorState err;
		if (!binding->init(err))
		{
			Logger::error("createBinding: %s", err.toString().c_str());
			return nullptr;
		}
		mBindings.emplace_back(binding);
		markDirty();
		return binding.get();
	}


	void lxcontrolService::removeControl(lx::Control* control)
	{
		mControls.erase(std::remove_if(mControls.begin(), mControls.end(),
			[control](const rtti::ObjectPtr<lx::Control>& c) { return c.get() == control; }), mControls.end());

		// Scrub any now-dangling ControlMapping referencing this control.
		eraseControlMappingsIf([control](const lx::ControlMapping& m) { return m.mControl.get() == control; });
		markDirty();
	}


	void lxcontrolService::removeBinding(lx::MidiBinding* binding)
	{
		mBindings.erase(std::remove_if(mBindings.begin(), mBindings.end(),
			[binding](const rtti::ObjectPtr<lx::MidiBinding>& b) { return b.get() == binding; }), mBindings.end());
		markDirty();
	}


	lx::Program* lxcontrolService::createProgram(const std::string& name)
	{
		auto program = mResourceManager->createObject<lx::Program>();
		program->mID = makeUniqueID("Program_" + name);
		program->mName = name;
		utility::ErrorState err;
		if (!program->init(err))
		{
			Logger::error("createProgram: %s", err.toString().c_str());
			return nullptr;
		}
		mPrograms.emplace_back(program);
		markDirty();
		return program.get();
	}


	void lxcontrolService::setProgramLifecycleTriggers(lx::Program& program, const std::vector<rtti::ObjectPtr<lx::Trigger>>& triggers)
	{
		program.mLifecycleTriggers.clear();
		for (auto& t : triggers)
			program.mLifecycleTriggers.emplace_back(nap::ResourcePtr<lx::Trigger>(t.get()));
		markDirty();
	}


	void lxcontrolService::removeProgram(lx::Program* program)
	{
		if (mActiveProgram == program)
			unloadProgram();

		// Drop every ControlMapping this program owned from the service's flat cache too.
		eraseControlMappingsIf([program](const lx::ControlMapping& m)
		{
			for (auto& pm : program->mControlMappings)
				if (pm.get() == &m) return true;
			return false;
		});

		mPrograms.erase(std::remove_if(mPrograms.begin(), mPrograms.end(),
			[program](const rtti::ObjectPtr<lx::Program>& p) { return p.get() == program; }), mPrograms.end());
		markDirty();
	}


	void lxcontrolService::eraseControlMappingsIf(const std::function<bool(const lx::ControlMapping&)>& pred)
	{
		mControlMappings.erase(std::remove_if(mControlMappings.begin(), mControlMappings.end(),
			[&pred](const rtti::ObjectPtr<lx::ControlMapping>& m) { return pred(*m); }), mControlMappings.end());
		for (auto& program : mPrograms)
		{
			auto& list = program->mControlMappings;
			list.erase(std::remove_if(list.begin(), list.end(),
				[&pred](const nap::ResourcePtr<lx::ControlMapping>& m) { return pred(*m); }), list.end());
		}
	}


	lx::ControlMapping* lxcontrolService::setControlMapping(lx::Program& program, lx::Control& control, lx::Trigger* trigger)
	{
		// Replace any existing mapping for this (Program, Control) pair first (also saves).
		clearControlMapping(program, control);
		if (trigger == nullptr)
			return nullptr;

		auto mapping = mResourceManager->createObject<lx::ControlMapping>();
		mapping->mID = makeUniqueID(program.mID + "_" + control.mID + "_Mapping");
		mapping->mControl = &control;
		mapping->mTrigger = trigger;
		utility::ErrorState err;
		if (!mapping->init(err))
		{
			Logger::error("setControlMapping: %s", err.toString().c_str());
			return nullptr;
		}
		mControlMappings.emplace_back(mapping);
		program.mControlMappings.emplace_back(nap::ResourcePtr<lx::ControlMapping>(mapping.get()));
		markDirty();
		return mapping.get();
	}


	void lxcontrolService::clearControlMapping(lx::Program& program, lx::Control& control)
	{
		eraseControlMappingsIf([&program, &control](const lx::ControlMapping& m)
		{
			if (m.mControl.get() != &control) return false;
			for (auto& pm : program.mControlMappings)
				if (pm.get() == &m) return true;
			return false;
		});
		markDirty();
	}


	lx::Trigger* lxcontrolService::getControlMapping(const lx::Program& program, const lx::Control& control) const
	{
		for (auto& pm : program.mControlMappings)
		{
			if (pm != nullptr && pm->mControl.get() == &control)
				return pm->mTrigger.get();
		}
		return nullptr;
	}


	lx::ControlMapping* lxcontrolService::routeControl(lx::Program& program, lx::Control& control, lx::Patch* patch,
		const std::vector<nap::ResourcePtr<lx::FixtureGroup>>& groups)
	{
		// A routing owns its own Control-kind trigger with exactly one binding (the UI never shows it).
		lx::Trigger* trig = createTrigger(lx::ETriggerKind::Control, control.mName);
		if (trig == nullptr)
			return nullptr;
		lx::PatchFixtureBinding b;
		b.mPatch = patch;
		b.mGroups = groups;
		trig->mBindings = { b };
		return setControlMapping(program, control, trig);
	}


	void lxcontrolService::setRoutingPatch(lx::Trigger& trigger, lx::Patch* patch)
	{
		lx::PatchFixtureBinding b;
		b.mPatch = patch;
		if (!trigger.mBindings.empty())
		{
			b.mGroups = trigger.mBindings[0].mGroups;				// keep groups + spread
			b.mEndToEnd = trigger.mBindings[0].mEndToEnd;
		}
		setTriggerBindings(trigger, { b });
	}


	void lxcontrolService::setRoutingSpread(lx::Trigger& trigger, bool endToEnd)
	{
		if (trigger.mBindings.empty())
			return;
		lx::PatchFixtureBinding b = trigger.mBindings[0];
		b.mEndToEnd = endToEnd;
		setTriggerBindings(trigger, { b });
	}


	void lxcontrolService::setRoutingGroups(lx::Trigger& trigger, const std::vector<nap::ResourcePtr<lx::FixtureGroup>>& groups)
	{
		lx::PatchFixtureBinding b;
		if (!trigger.mBindings.empty())
		{
			b.mPatch = trigger.mBindings[0].mPatch;					// keep patch + spread
			b.mEndToEnd = trigger.mBindings[0].mEndToEnd;
		}
		b.mGroups = groups;
		setTriggerBindings(trigger, { b });
	}


	void lxcontrolService::unroute(lx::Program& program, lx::ControlMapping* mapping)
	{
		if (mapping == nullptr)
			return;
		// removeTrigger scrubs every ControlMapping referencing it (and lifecycle lists), so the
		// dedicated trigger + this mapping both go away.
		lx::Trigger* trig = mapping->mTrigger.get();
		if (trig != nullptr)
			removeTrigger(trig);
		else if (mapping->mControl != nullptr)
			clearControlMapping(program, *mapping->mControl.get());
	}


	lx::Trigger* lxcontrolService::getLifecycleTrigger(const lx::Program& program, lx::ETriggerKind kind) const
	{
		for (auto& t : program.mLifecycleTriggers)
			if (t != nullptr && t->mKind == kind)
				return t.get();
		return nullptr;
	}


	lx::Trigger* lxcontrolService::ensureLifecycleTrigger(lx::Program& program, lx::ETriggerKind kind)
	{
		if (lx::Trigger* existing = getLifecycleTrigger(program, kind))
			return existing;
		lx::Trigger* trig = createTrigger(kind, kind == lx::ETriggerKind::Enter ? "OnLoad" : "OnExit");
		if (trig == nullptr)
			return nullptr;
		auto list = program.mLifecycleTriggers;
		list.emplace_back(nap::ResourcePtr<lx::Trigger>(trig));
		setProgramLifecycleTriggers(program, list);
		return trig;
	}


	void lxcontrolService::clearLifecycle(lx::Program& program, lx::ETriggerKind kind)
	{
		if (lx::Trigger* t = getLifecycleTrigger(program, kind))
			removeTrigger(t);	// also drops it from the program's lifecycle list
	}


	std::vector<std::string> lxcontrolService::fixtureClaimants(const lx::FixtureComponentInstance& fx) const
	{
		struct C { std::string mName; bool mHeld; uint64_t mId; };
		std::vector<C> claimants;
		const std::string eid = fx.getEntityID();
		for (const auto& act : mActivations)
		{
			if (act.mTrigger == nullptr)
				continue;
			for (const auto& b : act.mTrigger->mBindings)
			{
				bool drives = false;
				for (const auto& g : b.mGroups)
				{
					if (g == nullptr)
						continue;
					if (std::find(g->mFixtureNames.begin(), g->mFixtureNames.end(), eid) != g->mFixtureNames.end())
						{ drives = true; break; }
				}
				if (!drives)
					continue;
				claimants.push_back({ b.mPatch != nullptr ? b.mPatch->mName : std::string("(patch)"), act.mHeld, act.mId });
			}
		}
		// Held first, then newest first -- the same priority resolveValue() arbitrates per channel.
		std::sort(claimants.begin(), claimants.end(), [](const C& a, const C& b)
		{
			if (a.mHeld != b.mHeld) return a.mHeld && !b.mHeld;
			return a.mId > b.mId;
		});
		std::vector<std::string> out;
		for (auto& c : claimants)
			if (std::find(out.begin(), out.end(), c.mName) == out.end())
				out.push_back(c.mName);
		return out;
	}


	void lxcontrolService::loadProgram(lx::Program* program)
	{
		// Unload the outgoing program: fire its Exit triggers (transient look, rings out) and stop the rest.
		if (mActiveProgram != nullptr)
		{
			for (auto& t : mActiveProgram->mLifecycleTriggers)
			{
				if (t == nullptr) continue;
				if (t->mKind == lx::ETriggerKind::Exit)
					fireTrigger(*t);
				else
					stopTrigger(*t);
			}
		}

		mActiveProgram = program;

		// Load the incoming program: fire its Enter triggers. Control-kind triggers respond via onMidiEvent
		// only while this program is active (see the gate there).
		if (program != nullptr)
		{
			for (auto& t : program->mLifecycleTriggers)
			{
				if (t != nullptr && t->mKind == lx::ETriggerKind::Enter)
					fireTrigger(*t);
			}
		}
		saveSession();
	}


	void lxcontrolService::unloadProgram()
	{
		if (mActiveProgram == nullptr)
			return;
		for (auto& t : mActiveProgram->mLifecycleTriggers)
		{
			if (t == nullptr) continue;
			if (t->mKind == lx::ETriggerKind::Exit)
				fireTrigger(*t);
			else
				stopTrigger(*t);
		}
		mActiveProgram = nullptr;
		saveSession();
	}


	void lxcontrolService::save()
	{
		// Persist only authored data (patches + params + modulators). The per-modulator player graph is
		// runtime-only and reconstructed on load by buildModulatorGraph.
		rtti::ObjectList root_objects;
		for (auto& entry : mPatchEntries)
		{
			if (entry.mRemoved)
				continue;
			root_objects.emplace_back(entry.mPatch.get());
			for (auto& p : entry.mParams)
				root_objects.emplace_back(p.get());
			for (auto& me : entry.mModulators)
			{
				root_objects.emplace_back(me.mModulator.get());
				for (auto& in : me.mInputs)		// Field inputs: Default-pointer targets, must be listed
					root_objects.emplace_back(in.get());
			}
		}

		for (auto& trigger : mTriggers)
			root_objects.emplace_back(trigger.get());
		for (auto& control : mControls)
			root_objects.emplace_back(control.get());
		for (auto& binding : mBindings)
			root_objects.emplace_back(binding.get());
		for (auto& program : mPrograms)
			root_objects.emplace_back(program.get());
		for (auto& mapping : mControlMappings)
			root_objects.emplace_back(mapping.get());
		// Groups are Default-pointer targets from Trigger bindings; serializeObjects does NOT pull those
		// in, so an unlisted group would become a dangling id on the next load.
		for (auto& group : mGroups)
			root_objects.emplace_back(group.get());

		rtti::JSONWriter writer;
		utility::ErrorState error;
		if (!rtti::serializeObjects(root_objects, writer, error))
		{
			Logger::error("lxcontrolService: failed to serialize user content: %s", error.toString().c_str());
			return;
		}
		std::ofstream out(sUserContentFile);
		out << writer.GetJSON();
	}


	void lxcontrolService::onMidiEvent(const MidiEvent& event)
	{
		mMidiLog.push_front(event.toString());
		while (mMidiLog.size() > sMaxMidiLogSize)
			mMidiLog.pop_back();

		mLastEventType = event.getType();
		mLastEventNumber = event.getNumber();
		mLastEventValue = event.getValue();
		mLastEventChannel = event.getChannel();
		mLastEventPort = event.getPort();
		mHasLastEvent = true;
		mMidiEventCounter++;

		// Dispatch to controls via matching bindings. Which Trigger a Control fires (if any) is
		// resolved per active-Program via getControlMapping() — a direct lookup into that Program's
		// mControlMappings, replacing the old fixed control->mTrigger link.
		bool on_event = false;
		bool off_event = false;
		switch (event.getType())
		{
		case MidiEvent::Type::noteOn:			on_event = true;	break;
		case MidiEvent::Type::noteOff:			off_event = true;	break;
		case MidiEvent::Type::controlChange:	(event.getValue() >= 64 ? on_event : off_event) = true;	break;
		default:								on_event = true;	break;
		}

		for (auto& binding : mBindings)
		{
			if (!binding->matches(event))
				continue;
			lx::Control* control = binding->mControl.get();
			if (control == nullptr || mActiveProgram == nullptr)
				continue;

			// Program-scoped: a control only responds to whatever Trigger the active Program maps
			// it to; no mapping in this Program means this Control does nothing here.
			lx::Trigger* trig = getControlMapping(*mActiveProgram, *control);
			if (trig == nullptr)
				continue;

			switch (control->mMode)
			{
			case lx::EControlMode::Momentary:
				if (on_event && !control->mHeld)		{ control->mHeld = true;  fireTrigger(*trig, /*held*/true); }
				else if (off_event && control->mHeld)	{ control->mHeld = false; stopTrigger(*trig); }
				break;
			case lx::EControlMode::Toggle:
				if (on_event)
				{
					control->mLatched = !control->mLatched;
					if (control->mLatched) fireTrigger(*trig, /*held*/true); else stopTrigger(*trig);
				}
				break;
			case lx::EControlMode::FireOnly:
				if (on_event) fireTrigger(*trig, /*held*/false);
				break;
			}
		}
	}


	void lxcontrolService::authorFloatCurve(SequenceEditor& editor, const std::string& trackID,
		const std::vector<lx::Key>& keys)
	{
		if (keys.size() < 2)
			return;

		auto& ctrl = editor.getController<SequenceControllerCurve>();
		SequencePlayer* player = editor.mSequencePlayer.get();
		if (player == nullptr)
			return;

		// Find the track and clear any existing (auto-created) segments so authoring is deterministic.
		const SequenceTrack* track = nullptr;
		for (const auto& t : player->getSequenceConst().mTracks)
		{
			if (t->mID == trackID) { track = t.get(); break; }
		}
		if (track == nullptr)
		{
			Logger::error("authorFloatCurve: track %s not found", trackID.c_str());
			return;
		}

		std::vector<std::string> existing;
		for (const auto& s : track->mSegments)
			existing.emplace_back(s->mID);
		for (const auto& sid : existing)
			ctrl.deleteSegment(trackID, sid);

		// One contiguous segment per interval [t_i, t_{i+1}]. insertSegment on an empty track creates
		// [0, t]; subsequent inserts at increasing times append. It returns the new segment pointer.
		std::vector<std::string> seg_ids;
		for (size_t i = 1; i < keys.size(); ++i)
		{
			const SequenceTrackSegment* seg = ctrl.insertSegment(trackID, keys[i].mTime);
			if (seg == nullptr)
			{
				Logger::error("authorFloatCurve: insertSegment at %.3f failed", keys[i].mTime);
				return;
			}
			seg_ids.emplace_back(seg->mID);
		}

		// Values (0..1) + interpolation. updateCurveSegments forces each segment's start = previous end,
		// so set the first segment's BEGIN and every segment's END.
		ctrl.changeCurveSegmentValue(trackID, seg_ids.front(), keys.front().mValue, 0, sequencecurveenums::BEGIN);
		for (size_t i = 1; i < keys.size(); ++i)
		{
			ctrl.changeCurveSegmentValue(trackID, seg_ids[i - 1], keys[i].mValue, 0, sequencecurveenums::END);
			ctrl.changeCurveType(trackID, seg_ids[i - 1], keys[i].mInterp, 0);
		}

		editor.changeSequenceDuration(keys.back().mTime);
	}
}
