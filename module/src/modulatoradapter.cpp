#include "modulatoradapter.h"
#include "modulatoroutput.h"
#include "modulator.h"

#include <parameternumeric.h>

namespace lx
{
	ModulatorAdapter::ModulatorAdapter(ModulatorOutput& output) : mOutput(output)
	{
		mOutput.registerAdapter(this);
	}


	void ModulatorAdapter::tick(double time)
	{
		// Clock/player thread: compute the modulator value from its own elapsed clock (advanced on the
		// main thread in Effect::update), store it under lock. The player time is only a heartbeat here.
		float value = mOutput.mModulator != nullptr ? mOutput.mModulator->evaluate() : 0.0f;
		std::unique_lock<std::mutex> lock(mMutex);
		mStoredValue = value;
	}


	void ModulatorAdapter::flush()
	{
		std::unique_lock<std::mutex> lock(mMutex);
		if (mOutput.mParameter != nullptr)
			mOutput.mParameter->setValue(mStoredValue);
	}


	void ModulatorAdapter::destroy()
	{
		mOutput.removeAdapter(this);
	}
}
