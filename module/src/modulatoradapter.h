#pragma once

// External Includes
#include <sequenceplayeradapter.h>
#include <mutex>

namespace lx
{
	class ModulatorOutput;

	/**
	 * Custom napsequence adapter for a procedural modulator. tick() (clock/player thread) calls the
	 * owning modulator's evaluate() and stores the result under a mutex; flush() (main thread, driven
	 * by ModulatorOutput::update) writes it into the sink parameter. Mirrors the curve adapter's
	 * main-thread hand-off exactly — the parameter is never written off the main thread.
	 */
	class ModulatorAdapter : public nap::SequencePlayerAdapter
	{
	public:
		ModulatorAdapter(ModulatorOutput& output);

		void tick(double time) override;
		void destroy() override;

		/** Main thread: push the last ticked value into the output's sink parameter. */
		void flush();

	private:
		ModulatorOutput&	mOutput;
		std::mutex			mMutex;
		float				mStoredValue = 0.0f;
	};
}
