// Local Includes
#include "midihotplugmonitor.h"

// External Includes
#include <RtMidi.h>
#include <set>

namespace nap
{
	MidiHotplugMonitor::MidiHotplugMonitor()
	{
		try
		{
			mPollMidiIn = std::make_unique<RtMidiIn>();
			mLastPorts = pollPortNames();
		}
		catch (RtMidiError&)
		{
			mPollMidiIn.reset();
		}
	}


	MidiHotplugMonitor::~MidiHotplugMonitor()
	{
	}


	std::vector<std::string> MidiHotplugMonitor::pollPortNames() const
	{
		std::vector<std::string> names;
		if (mPollMidiIn == nullptr)
			return names;

		try
		{
			unsigned int port_count = mPollMidiIn->getPortCount();
			for (unsigned int i = 0; i < port_count; ++i)
				names.emplace_back(mPollMidiIn->getPortName(i));
		}
		catch (RtMidiError&)
		{
			// A device change mid-query can throw; treat as 'nothing new to report' this poll
		}
		return names;
	}


	bool MidiHotplugMonitor::update(double deltaTime, std::vector<std::string>& newPorts)
	{
		mAccumTime += deltaTime;
		if (mAccumTime < sPollIntervalSeconds)
			return false;
		mAccumTime = 0.0;

		std::vector<std::string> polled = pollPortNames();

		std::set<std::string> polled_set(polled.begin(), polled.end());
		std::set<std::string> last_set(mLastPorts.begin(), mLastPorts.end());
		if (polled_set == last_set)
			return false;

		mLastPorts = polled;
		newPorts = polled;
		return true;
	}
}
