#include "modulatoroutput.h"
#include "modulatoradapter.h"

#include <sequenceservice.h>
#include <algorithm>

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(lx::ModulatorOutput)
	RTTI_CONSTRUCTOR(nap::SequenceService&)
	RTTI_PROPERTY("Parameter", &lx::ModulatorOutput::mParameter, nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace lx
{
	ModulatorOutput::ModulatorOutput(nap::SequenceService& service) : nap::SequencePlayerOutput(service)
	{
	}


	void ModulatorOutput::registerAdapter(ModulatorAdapter* adapter)
	{
		if (std::find(mAdapters.begin(), mAdapters.end(), adapter) == mAdapters.end())
			mAdapters.emplace_back(adapter);
	}


	void ModulatorOutput::removeAdapter(ModulatorAdapter* adapter)
	{
		mAdapters.erase(std::remove(mAdapters.begin(), mAdapters.end(), adapter), mAdapters.end());
	}


	void ModulatorOutput::update(double deltaTime)
	{
		// Main thread: push each adapter's thread-stored value into the sink parameter.
		for (auto* adapter : mAdapters)
			adapter->flush();
	}
}
