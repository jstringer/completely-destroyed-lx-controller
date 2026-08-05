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


	bool Patch::isFieldInput(const PatchParameter* p) const
	{
		if (p == nullptr)
			return false;
		for (auto& modulator : mModulators)
		{
			auto* field = rtti_cast<FieldModulator>(modulator.get());
			if (field == nullptr)
				continue;
			for (auto* in : field->inputs())
				if (in == p)
					return true;
		}
		return false;
	}


	void Patch::update(double deltaTime)
	{
		// Transport housekeeping (sustain pause, one-shot end-stop). Source values are not precomputed
		// anywhere: a channel claim calls evaluate() when it needs one.
		for (auto& modulator : mModulators)
			modulator->update(deltaTime);

		// Pass 1: modulators whose targets are Field INPUTS write them now, so every evaluate() this frame
		// (and every Field reading its own inputs) sees the driven value.
		//
		// One pass per frame, non-recursive: a user-authored A<->B pair between two inputs resolves with one
		// frame of latency instead of hanging, so no cycle detection is needed at all.
		for (auto& modulator : mModulators)
		{
			for (auto& target : modulator->mTargets)
			{
				PatchParameter* input = target.get();
				if (input == nullptr || !isFieldInput(input))
					continue;
				if (auto* fp = rtti_cast<FloatParameter>(input))
					fp->mValue = nap::math::clamp(modulator->value(0.0f, 0), 0.0f, 1.0f);
			}
		}
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
