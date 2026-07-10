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
		// Clock/player thread: compute the modulator value procedurally, store it under lock.
		float value = mOutput.mModulator != nullptr ? mOutput.mModulator->evaluate(time) : 0.0f;
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
