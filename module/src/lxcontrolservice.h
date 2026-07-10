#pragma once

// External Includes
#include <nap/service.h>
#include <nap/resourceptr.h>
#include <nap/signalslot.h>
#include <midievent.h>
#include <midiinputcomponent.h>
#include <midiport/midiinputport.h>
#include <deque>
#include <vector>

// Local Includes
#include "midihotplugmonitor.h"

namespace lx { class FixtureComponentInstance; }

namespace nap
{
	class ResourceManager;

	/**
	 * Runtime authority for the lxcontrol app.
	 *
	 * Phase 1 surface: owns the fixture registry (fixtures self-register from their
	 * lx::FixtureComponentInstance::init), the wildcard MIDI-listener subscription, MIDI hot-plug
	 * reconnect, and the MIDI log / learn snapshot. Effects, Triggers, Controllers and Programs are
	 * added in later phases.
	 */
	class NAPAPI lxcontrolService : public Service
	{
		RTTI_ENABLE(Service)
	public:
		lxcontrolService(ServiceConfiguration* configuration) : Service(configuration) { }

		virtual void getDependentServices(std::vector<rtti::TypeInfo>& dependencies) override;
		virtual bool init(utility::ErrorState& errorState) override;
		virtual void update(double deltaTime) override;
		virtual void shutdown() override;

		/**
		 * Called once from lxcontrolApp::init(), after objects.json has loaded. Connects to the
		 * wildcard MIDI listener and remembers the MIDI port for hot-plug reconnect.
		 */
		bool setup(MidiInputComponentInstance& midiSource, ResourcePtr<MidiInputPort> midiPort, utility::ErrorState& errorState);

		// --- Fixture registry (fixtures self-register from their component init) ---
		void registerFixture(lx::FixtureComponentInstance* fixture);
		void unregisterFixture(lx::FixtureComponentInstance* fixture);
		const std::vector<lx::FixtureComponentInstance*>& getFixtures() const { return mFixtures; }

		// --- MIDI log / learn ---
		const std::deque<std::string>& getMidiLog() const { return mMidiLog; }
		bool hasLastMidiEvent() const { return mHasLastEvent; }
		MidiEvent getLastMidiEvent() const { return MidiEvent(mLastEventType, mLastEventNumber, mLastEventValue, mLastEventChannel, mLastEventPort); }
		int getMidiEventCounter() const { return mMidiEventCounter; }

	private:
		void onMidiEvent(const MidiEvent& event);

		ResourceManager*					mResourceManager = nullptr;
		std::vector<lx::FixtureComponentInstance*>	mFixtures;

		ResourcePtr<MidiInputPort>			mMidiPort;
		std::unique_ptr<MidiHotplugMonitor>	mMidiHotplugMonitor;

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
