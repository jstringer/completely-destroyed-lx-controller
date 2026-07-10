// Local Includes
#include "lxcontrolservice.h"
#include "modulatortrack.h"
#include "modulatoradapter.h"
#include "fixturecomponent.h"
#include "fixturechannelcomponent.h"

// External Includes
#include <nap/core.h>
#include <nap/resourcemanager.h>
#include <nap/logger.h>
#include <rtti/factory.h>
#include <rtti/jsonwriter.h>
#include <rtti/writer.h>
#include <sequenceservice.h>
#include <sequence.h>
#include <utility/fileutils.h>
#include <algorithm>
#include <fstream>

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::lxcontrolService)
	RTTI_CONSTRUCTOR(nap::ServiceConfiguration*)
RTTI_END_CLASS

namespace nap
{
	static constexpr const char* sUserContentFile = "user_content.json";


	void lxcontrolService::getDependentServices(std::vector<rtti::TypeInfo>& dependencies)
	{
		dependencies.emplace_back(RTTI_OF(SequenceService));
	}


	void lxcontrolService::registerObjectCreators(rtti::Factory& factory)
	{
		// ModulatorOutput's ctor needs a SequenceService&, so it requires a custom ObjectCreator.
		auto* seq = getCore().getService<SequenceService>();
		if (seq != nullptr)
			factory.addObjectCreator(std::make_unique<lx::ModulatorOutputObjectCreator>(*seq));
	}


	bool lxcontrolService::init(nap::utility::ErrorState& errorState)
	{
		mResourceManager = getCore().getResourceManager();

		// Register the custom modulator adapter (keyed by ModulatorTrack) and the default-track-creator
		// (keyed by ModulatorOutput) so an empty modulator player auto-gets a ModulatorTrack on init
		// and a ModulatorAdapter on start. Must happen before any modulator player starts.
		auto* seq = getCore().getService<SequenceService>();
		if (seq != nullptr)
		{
			seq->registerAdapterFactoryFunc(RTTI_OF(lx::ModulatorTrack),
				[](const SequenceTrack& track, SequencePlayerOutput& output, const SequencePlayer& player) -> std::unique_ptr<SequencePlayerAdapter>
				{
					auto& mod_output = static_cast<lx::ModulatorOutput&>(output);
					return std::make_unique<lx::ModulatorAdapter>(mod_output);
				});

			seq->registerDefaultTrackCreatorForOutput(RTTI_OF(lx::ModulatorOutput),
				[](const SequencePlayerOutput* output) -> std::unique_ptr<SequenceTrack>
				{
					auto track = std::make_unique<lx::ModulatorTrack>();
					track->mAssignedOutputID = output->mID;
					return track;
				});
		}
		return true;
	}


	void lxcontrolService::update(double deltaTime)
	{
		// Drive every effect so its modulators (fed by their player clocks / adapters) blend into the
		// effect parameters each frame.
		for (auto& entry : mEffectEntries)
		{
			if (!entry.mRemoved && entry.mEffect != nullptr)
				entry.mEffect->update(deltaTime);
		}

		// Reap releasing activations whose effects have all finished (release-linger): their claims
		// stay until then, so a stopped ADSR rings out before the channel reverts.
		for (auto it = mActivations.begin(); it != mActivations.end(); )
		{
			bool finished = it->mReleasing;
			for (auto* e : it->mEffects)
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
	}


	bool lxcontrolService::setup(MidiInputComponentInstance& midiSource, ResourcePtr<MidiInputPort> midiPort, utility::ErrorState& errorState)
	{
		mMidiPort = midiPort;
		mMidiHotplugMonitor = std::make_unique<MidiHotplugMonitor>();
		midiSource.messageReceived.connect(mMidiSlot);

		if (utility::fileExists(sUserContentFile))
		{
			if (!mResourceManager->loadFile(sUserContentFile, errorState))
			{
				// Unloadable (e.g. old-format) content must never halt startup: rename and continue empty.
				std::string backup = std::string(sUserContentFile) + ".bak";
				std::remove(backup.c_str());
				std::rename(sUserContentFile, backup.c_str());
				Logger::warn("lxcontrolService: could not load %s (%s); renamed to %s, continuing empty",
					sUserContentFile, errorState.toString().c_str(), backup.c_str());
				return true;
			}
			rebuildFromLoadedContent();
		}
		return true;
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


	std::string lxcontrolService::makeUniqueID(const std::string& base) const
	{
		if (mResourceManager->findObject(base) == nullptr)
			return base;
		int suffix = 2;
		while (true)
		{
			std::string candidate = base + "_" + std::to_string(suffix);
			if (mResourceManager->findObject(candidate) == nullptr)
				return candidate;
			suffix++;
		}
	}


	lxcontrolService::EffectEntry* lxcontrolService::findEntry(lx::Effect& effect)
	{
		for (auto& entry : mEffectEntries)
		{
			if (entry.mEffect.get() == &effect)
				return &entry;
		}
		return nullptr;
	}


	lx::Effect* lxcontrolService::createEffect(const std::string& name)
	{
		auto effect = mResourceManager->createObject<lx::Effect>();
		effect->mID = makeUniqueID("Effect_" + name);
		effect->mName = name;

		utility::ErrorState err;
		if (!effect->init(err))
		{
			Logger::error("createEffect: %s", err.toString().c_str());
			return nullptr;
		}

		EffectEntry entry;
		entry.mEffect = effect;
		mEffectEntries.emplace_back(entry);
		mEffects.emplace_back(effect);
		save();
		return effect.get();
	}


	lx::EffectParameter* lxcontrolService::addEffectParameter(lx::Effect& effect, rtti::TypeInfo type)
	{
		EffectEntry* entry = findEntry(effect);
		if (entry == nullptr)
			return nullptr;

		auto obj = mResourceManager->createObject(type);
		auto* param = rtti_cast<lx::EffectParameter>(obj.get());
		if (param == nullptr)
		{
			Logger::error("addEffectParameter: type %s is not an EffectParameter", type.get_name().data());
			return nullptr;
		}

		param->mID = makeUniqueID(effect.mID + "_Param");
		param->mName = "Param";
		utility::ErrorState err;
		if (!param->init(err))
		{
			Logger::error("addEffectParameter: %s", err.toString().c_str());
			return nullptr;
		}

		effect.mParameters.emplace_back(param);
		entry->mParams.emplace_back(rtti::ObjectPtr<lx::EffectParameter>(param));
		save();
		return param;
	}


	bool lxcontrolService::buildModulatorGraph(ModulatorEntry& entry, const std::string& base, utility::ErrorState& err)
	{
		lx::Modulator* mod = entry.mModulator.get();

		// ponytail: StandardClock only in Phase 2 (proven via createObject); IndependentClock deferred.
		auto clock = mResourceManager->createObject<SequencePlayerStandardClock>();
		clock->mID = makeUniqueID(base + "_Clock");
		if (!clock->init(err))
			return false;
		entry.mClock = clock;

		auto sink = mResourceManager->createObject<ParameterFloat>();
		sink->mID = makeUniqueID(base + "_Sink");
		sink->mMinimum = 0.0f; sink->mMaximum = 1.0f; sink->mValue = 0.0f;
		if (!sink->init(err))
			return false;
		entry.mSink = sink;

		auto output = mResourceManager->createObject<lx::ModulatorOutput>();
		output->mID = makeUniqueID(base + "_Output");
		output->mParameter = sink;
		output->mModulator = mod;
		if (!output->init(err))
			return false;
		entry.mOutput = output;

		auto player = mResourceManager->createObject<SequencePlayer>();
		player->mID = makeUniqueID(base + "_Player");
		player->mSequenceFileName = base + ".json";	// non-existent -> empty sequence + default ModulatorTrack
		player->mCreateEmptySequenceOnLoadFail = true;
		player->mClock = entry.mClock;
		player->mOutputs.emplace_back(rtti::ObjectPtr<SequencePlayerOutput>(output.get()));
		if (!player->init(err))
			return false;
		if (!player->start(err))
			return false;
		entry.mPlayer = player;

		// The empty default sequence has duration 0, so a non-looping player stops immediately and never
		// ticks the adapter again. Give it a real duration and loop it so it keeps ticking forever; the
		// modulator's value comes from its own elapsed clock (Modulator::advance), not the player time.
		auto editor = mResourceManager->createObject<SequenceEditor>();
		editor->mID = makeUniqueID(base + "_Editor");
		editor->mSequencePlayer = player;
		if (!editor->init(err))
			return false;
		editor->changeSequenceDuration(3600.0);
		entry.mEditor = editor;

		mod->mPlayer = player.get();
		mod->mSink = sink.get();
		player->setIsLooping(true);
		player->setPlayerTime(0.0);
		player->setIsPlaying(true);
		return true;
	}


	lx::Modulator* lxcontrolService::addModulator(lx::Effect& effect, rtti::TypeInfo type, lx::EffectParameter* target)
	{
		EffectEntry* effect_entry = findEntry(effect);
		if (effect_entry == nullptr)
			return nullptr;

		auto obj = mResourceManager->createObject(type);
		auto* mod = rtti_cast<lx::Modulator>(obj.get());
		if (mod == nullptr)
		{
			Logger::error("addModulator: type %s is not a Modulator", type.get_name().data());
			return nullptr;
		}

		mod->mID = makeUniqueID(effect.mID + "_Mod");
		mod->mName = std::string(type.get_name().data());
		mod->mTarget = target;
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

		effect.mModulators.emplace_back(mod);
		effect_entry->mModulators.emplace_back(mod_entry);
		save();
		return mod;
	}


	void lxcontrolService::removeEffect(lx::Effect* effect)
	{
		// Delete-path guard: reap any activation that references this effect so no stale claim pins a
		// channel to a frozen value.
		for (auto it = mActivations.begin(); it != mActivations.end(); )
		{
			bool references = std::find(it->mEffects.begin(), it->mEffects.end(), effect) != it->mEffects.end();
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

		for (auto& entry : mEffectEntries)
		{
			if (entry.mEffect.get() != effect)
				continue;
			for (auto& me : entry.mModulators)
			{
				if (me.mPlayer != nullptr)
					me.mPlayer->setIsPlaying(false);
			}
			entry.mRemoved = true;
			break;
		}
		mEffects.erase(std::remove_if(mEffects.begin(), mEffects.end(),
			[effect](const rtti::ObjectPtr<lx::Effect>& e) { return e.get() == effect; }), mEffects.end());
		save();
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


	lx::Trigger* lxcontrolService::createTrigger(rtti::TypeInfo type, const std::string& name)
	{
		auto obj = mResourceManager->createObject(type);
		auto* trigger = rtti_cast<lx::Trigger>(obj.get());
		if (trigger == nullptr)
		{
			Logger::error("createTrigger: type %s is not a Trigger", type.get_name().data());
			return nullptr;
		}
		trigger->mID = makeUniqueID("Trigger_" + name);
		trigger->mName = name;
		utility::ErrorState err;
		if (!trigger->init(err))
		{
			Logger::error("createTrigger: %s", err.toString().c_str());
			return nullptr;
		}
		mTriggers.emplace_back(trigger);
		save();
		return trigger;
	}


	void lxcontrolService::setTriggerBindings(lx::Trigger& trigger, const std::vector<lx::EffectFixtureBinding>& bindings)
	{
		trigger.mBindings = bindings;
		save();
	}


	void lxcontrolService::removeTrigger(lx::Trigger* trigger)
	{
		if (trigger != nullptr)
			stopTrigger(*trigger);
		mTriggers.erase(std::remove_if(mTriggers.begin(), mTriggers.end(),
			[trigger](const rtti::ObjectPtr<lx::Trigger>& t) { return t.get() == trigger; }), mTriggers.end());
		save();
	}


	uint64_t lxcontrolService::fireTrigger(lx::Trigger& trigger)
	{
		// Retrigger: stop-then-replace any existing activation of this trigger.
		stopTrigger(trigger);

		Activation activation;
		activation.mId = mNextActivationId++;
		activation.mTrigger = &trigger;

		for (auto& binding : trigger.mBindings)
		{
			lx::Effect* effect = binding.mEffect.get();
			if (effect == nullptr)
				continue;
			activation.mEffects.emplace_back(effect);

			for (auto& fixture_name : binding.mFixtureNames)
			{
				lx::FixtureComponentInstance* fixture = findFixture(fixture_name);
				if (fixture == nullptr)
					continue;

				for (auto* channel : fixture->getChannels())
				{
					for (auto& param : effect->mParameters)
					{
						int count = param->getComponentCount();
						for (int c = 0; c < count; ++c)
						{
							if (param->getComponentRole(c) == channel->getRole() && param->appliesToUnit(channel->getUnitIndex()))
								channel->pushClaim(activation.mId, param.get(), c);
						}
					}
				}
			}
			effect->trigger();
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
			for (auto* effect : activation.mEffects)
				effect->stop();
			activation.mReleasing = true;
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


	lx::Controller* lxcontrolService::createController(const std::string& name, lx::Trigger* trigger, lx::EControllerMode mode)
	{
		auto controller = mResourceManager->createObject<lx::Controller>();
		controller->mID = makeUniqueID("Controller_" + name);
		controller->mName = name;
		controller->mTrigger = trigger;
		controller->mMode = mode;
		utility::ErrorState err;
		if (!controller->init(err))
		{
			Logger::error("createController: %s", err.toString().c_str());
			return nullptr;
		}
		mControllers.emplace_back(controller);
		save();
		return controller.get();
	}


	lx::MidiBinding* lxcontrolService::createBinding(const MidiEvent& learnedEvent, lx::Controller& controller)
	{
		auto binding = mResourceManager->createObject<lx::MidiBinding>();
		binding->mID = makeUniqueID("Binding_" + std::to_string(mBindings.size() + 1));

		// Device-agnostic by default: message type + number only (no port/channel).
		binding->mNumbers = { static_cast<int>(learnedEvent.getNumber()) };
		switch (learnedEvent.getType())
		{
		// A note binding matches both on and off so Momentary controllers can release; Toggle/FireOnly
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
		binding->mController = &controller;

		utility::ErrorState err;
		if (!binding->init(err))
		{
			Logger::error("createBinding: %s", err.toString().c_str());
			return nullptr;
		}
		mBindings.emplace_back(binding);
		save();
		return binding.get();
	}


	void lxcontrolService::removeController(lx::Controller* controller)
	{
		mControllers.erase(std::remove_if(mControllers.begin(), mControllers.end(),
			[controller](const rtti::ObjectPtr<lx::Controller>& c) { return c.get() == controller; }), mControllers.end());
		save();
	}


	void lxcontrolService::removeBinding(lx::MidiBinding* binding)
	{
		mBindings.erase(std::remove_if(mBindings.begin(), mBindings.end(),
			[binding](const rtti::ObjectPtr<lx::MidiBinding>& b) { return b.get() == binding; }), mBindings.end());
		save();
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
		save();
		return program.get();
	}


	void lxcontrolService::setProgramTriggers(lx::Program& program, const std::vector<rtti::ObjectPtr<lx::Trigger>>& triggers)
	{
		program.mTriggers.clear();
		for (auto& t : triggers)
			program.mTriggers.emplace_back(nap::ResourcePtr<lx::Trigger>(t.get()));
		save();
	}


	void lxcontrolService::removeProgram(lx::Program* program)
	{
		if (mActiveProgram == program)
			unloadProgram();
		mPrograms.erase(std::remove_if(mPrograms.begin(), mPrograms.end(),
			[program](const rtti::ObjectPtr<lx::Program>& p) { return p.get() == program; }), mPrograms.end());
		save();
	}


	void lxcontrolService::loadProgram(lx::Program* program)
	{
		// Unload the outgoing program: fire its ExitTriggers (transient look, rings out) and stop the rest.
		if (mActiveProgram != nullptr)
		{
			for (auto& t : mActiveProgram->mTriggers)
			{
				if (t == nullptr) continue;
				if (rtti_cast<lx::ExitTrigger>(t.get()) != nullptr)
					fireTrigger(*t);
				else
					stopTrigger(*t);
			}
		}

		mActiveProgram = program;

		// Load the incoming program: fire its EnterTriggers. ControllerTriggers respond via onMidiEvent
		// only while this program is active (see the gate there).
		if (program != nullptr)
		{
			for (auto& t : program->mTriggers)
			{
				if (t != nullptr && rtti_cast<lx::EnterTrigger>(t.get()) != nullptr)
					fireTrigger(*t);
			}
		}
	}


	void lxcontrolService::unloadProgram()
	{
		if (mActiveProgram == nullptr)
			return;
		for (auto& t : mActiveProgram->mTriggers)
		{
			if (t == nullptr) continue;
			if (rtti_cast<lx::ExitTrigger>(t.get()) != nullptr)
				fireTrigger(*t);
			else
				stopTrigger(*t);
		}
		mActiveProgram = nullptr;
	}


	void lxcontrolService::save()
	{
		// Persist only authored data (effects + params + modulators). The per-modulator player graph is
		// runtime-only and reconstructed on load by buildModulatorGraph.
		rtti::ObjectList root_objects;
		for (auto& entry : mEffectEntries)
		{
			if (entry.mRemoved)
				continue;
			root_objects.emplace_back(entry.mEffect.get());
			for (auto& p : entry.mParams)
				root_objects.emplace_back(p.get());
			for (auto& me : entry.mModulators)
				root_objects.emplace_back(me.mModulator.get());
		}

		for (auto& trigger : mTriggers)
			root_objects.emplace_back(trigger.get());
		for (auto& controller : mControllers)
			root_objects.emplace_back(controller.get());
		for (auto& binding : mBindings)
			root_objects.emplace_back(binding.get());
		for (auto& program : mPrograms)
			root_objects.emplace_back(program.get());

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


	void lxcontrolService::rebuildFromLoadedContent()
	{
		utility::ErrorState err;
		for (auto& effect : mResourceManager->getObjects<lx::Effect>())
		{
			EffectEntry entry;
			entry.mEffect = effect;
			for (auto& p : effect->mParameters)
				entry.mParams.emplace_back(rtti::ObjectPtr<lx::EffectParameter>(p.get()));

			for (auto& m : effect->mModulators)
			{
				ModulatorEntry me;
				me.mModulator = rtti::ObjectPtr<lx::Modulator>(m.get());
				if (!buildModulatorGraph(me, makeUniqueID(m->mID + "_rt"), err))
					Logger::error("rebuild: modulator graph failed for %s: %s", m->mID.c_str(), err.toString().c_str());
				entry.mModulators.emplace_back(me);
			}

			mEffectEntries.emplace_back(entry);
			mEffects.emplace_back(effect);
		}

		for (auto& trigger : mResourceManager->getObjects<lx::Trigger>())
			mTriggers.emplace_back(trigger);
		for (auto& controller : mResourceManager->getObjects<lx::Controller>())
			mControllers.emplace_back(controller);
		for (auto& binding : mResourceManager->getObjects<lx::MidiBinding>())
			mBindings.emplace_back(binding);
		for (auto& program : mResourceManager->getObjects<lx::Program>())
			mPrograms.emplace_back(program);
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

		// Dispatch to controllers via matching bindings.
		// ponytail: armed unconditionally here; Phase 5 gates by active-program membership.
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
			lx::Controller* controller = binding->mController.get();
			if (controller == nullptr || controller->mTrigger == nullptr)
				continue;
			lx::Trigger* trig = controller->mTrigger.get();

			// Program-scoped: a controller only responds while its trigger belongs to the active program.
			if (mActiveProgram == nullptr)
				continue;
			bool in_program = false;
			for (auto& pt : mActiveProgram->mTriggers)
			{
				if (pt.get() == trig) { in_program = true; break; }
			}
			if (!in_program)
				continue;

			switch (controller->mMode)
			{
			case lx::EControllerMode::Momentary:
				if (on_event && !controller->mHeld)		{ controller->mHeld = true;  fireTrigger(*trig); }
				else if (off_event && controller->mHeld)	{ controller->mHeld = false; stopTrigger(*trig); }
				break;
			case lx::EControllerMode::Toggle:
				if (on_event)
				{
					controller->mLatched = !controller->mLatched;
					if (controller->mLatched) fireTrigger(*trig); else stopTrigger(*trig);
				}
				break;
			case lx::EControllerMode::FireOnly:
				if (on_event) fireTrigger(*trig);
				break;
			}
		}
	}
}
