// Local Includes
#include "lxcontrolservice.h"

// External Includes
#include <nap/core.h>
#include <nap/resourcemanager.h>
#include <nap/logger.h>
#include <rtti/factory.h>
#include <parameternumeric.h>
#include <sequenceservice.h>
#include <sequenceplayer.h>
#include <sequenceplayerclock.h>
#include <sequence.h>
#include <algorithm>

// Local Includes
#include "modulatortrack.h"
#include "modulatoroutput.h"
#include "modulatoradapter.h"

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::lxcontrolService)
	RTTI_CONSTRUCTOR(nap::ServiceConfiguration*)
RTTI_END_CLASS

namespace nap
{
	bool lxcontrolService::init(nap::utility::ErrorState& errorState)
	{
		mResourceManager = getCore().getResourceManager();

		// Register the custom modulator adapter (keyed by ModulatorTrack) and the default-track-creator
		// (keyed by ModulatorOutput) so an empty modulator player auto-gets a ModulatorTrack on init and
		// a ModulatorAdapter on start. Must happen before any modulator player starts.
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

		verifyModulatorSubstrate();
		return true;
	}


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


	void lxcontrolService::update(double deltaTime)
	{
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
	}


	void lxcontrolService::verifyModulatorSubstrate()
	{
		utility::ErrorState err;

		auto sink = mResourceManager->createObject<ParameterFloat>();
		sink->mID = "SelfCheck_ModSink";
		sink->mMinimum = 0.0f; sink->mMaximum = 1.0f; sink->mValue = 0.0f;
		if (!sink->init(err)) { Logger::error("[selfcheck] sink init: %s", err.toString().c_str()); return; }

		auto clock = mResourceManager->createObject<SequencePlayerStandardClock>();
		clock->mID = "SelfCheck_ModClock";
		if (!clock->init(err)) { Logger::error("[selfcheck] clock init: %s", err.toString().c_str()); return; }

		auto output = mResourceManager->createObject<lx::ModulatorOutput>();
		output->mID = "SelfCheck_ModOutput";
		output->mParameter = sink;
		output->mModulator = nullptr;
		if (!output->init(err)) { Logger::error("[selfcheck] output init: %s", err.toString().c_str()); return; }

		auto player = mResourceManager->createObject<SequencePlayer>();
		player->mID = "SelfCheck_ModPlayer";
		player->mSequenceFileName = "selfcheck_nonexistent.json";
		player->mCreateEmptySequenceOnLoadFail = true;
		player->mClock = clock;
		player->mOutputs.emplace_back(rtti::ObjectPtr<SequencePlayerOutput>(output.get()));
		if (!player->init(err)) { Logger::error("[selfcheck] player init: %s", err.toString().c_str()); return; }

		const Sequence& seq = player->getSequenceConst();
		Logger::info("[selfcheck] modulator bank sequence tracks: %d", static_cast<int>(seq.mTracks.size()));
		for (auto& t : seq.mTracks)
			Logger::info("[selfcheck] track id='%s' outputID='%s' type='%s'",
				t->mID.c_str(), t->mAssignedOutputID.c_str(), t->get_type().get_name().data());

		if (!player->start(err)) { Logger::error("[selfcheck] player start: %s", err.toString().c_str()); return; }
		Logger::info("[selfcheck] player started OK (adapter factory fired for ModulatorTrack)");
	}
}
