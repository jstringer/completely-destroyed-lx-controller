#pragma once

// External Includes
#include <vector>
#include <string>
#include <memory>

class RtMidiIn;

namespace nap
{
	/**
	 * Polls the system's available MIDI input ports on a timer and reports when the set of
	 * available ports changes, so a statically-declared nap::MidiInputPort (whose port list is
	 * otherwise only ever queried once, at start()) can be restarted to pick up newly connected
	 * or removed devices. Owns a dedicated RtMidiIn instance used purely for polling
	 * (getPortCount()/getPortName()) - it is never opened/started.
	 */
	class MidiHotplugMonitor final
	{
	public:
		MidiHotplugMonitor();
		~MidiHotplugMonitor();

		/**
		 * Advances the internal poll timer. Returns true - and fills newPorts with the current
		 * port names - only when the timer fired AND the available port set differs from the
		 * last-known set.
		 */
		bool update(double deltaTime, std::vector<std::string>& newPorts);

	private:
		std::vector<std::string> pollPortNames() const;

		std::unique_ptr<RtMidiIn>	mPollMidiIn;
		std::vector<std::string>	mLastPorts;
		double						mAccumTime = 0.0;

		static constexpr double sPollIntervalSeconds = 2.0;
	};
}
