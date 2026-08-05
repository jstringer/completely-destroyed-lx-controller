#include "patch.h"

#include <algorithm>

RTTI_BEGIN_CLASS(lx::Patch)
	RTTI_PROPERTY("Name",			&lx::Patch::mName,			nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Parameters",	&lx::Patch::mParameters,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Modulators",		&lx::Patch::mModulators,		nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace lx
{
	void Patch::trigger()
	{
		for (auto& modulator : mModulators)
			modulator->onTrigger();
	}


	void Patch::stop()
	{
		for (auto& modulator : mModulators)
			modulator->onStop();
	}


	void Patch::update(double deltaTime)
	{
		// Transport housekeeping only (sustain pause, one-shot end-stop). Values are no longer precomputed
		// anywhere: a channel claim calls evaluate() when it needs one, so nothing here has to know -- or
		// could get wrong -- how many sources this patch is currently driving.
		for (auto& modulator : mModulators)
			modulator->update(deltaTime);
	}


	bool Patch::isFinished() const
	{
		for (auto& modulator : mModulators)
		{
			if (!modulator->isFinished())
				return false;
		}
		return true;
	}
}
