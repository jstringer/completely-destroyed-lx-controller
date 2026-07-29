#include "patch.h"

#include <algorithm>

RTTI_BEGIN_ENUM(lx::EPatchTargetMode)
	RTTI_ENUM_VALUE(lx::EPatchTargetMode::Single,		"Single"),
	RTTI_ENUM_VALUE(lx::EPatchTargetMode::Multiple,	"Multiple")
RTTI_END_ENUM

RTTI_BEGIN_CLASS(lx::Patch)
	RTTI_PROPERTY("Name",			&lx::Patch::mName,			nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Parameters",	&lx::Patch::mParameters,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Modulators",		&lx::Patch::mModulators,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("TargetMode",		&lx::Patch::mTargetMode,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("FixtureCount",	&lx::Patch::mFixtureCount,	nap::rtti::EPropertyMetaData::Default)
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
		int voices = (mTargetMode == EPatchTargetMode::Multiple) ? std::max(1, mFixtureCount) : 1;

		for (auto& param : mParameters)
			param->resetToBase(voices);

		for (auto& modulator : mModulators)
		{
			modulator->update(deltaTime);	// per-frame transport housekeeping (sustain pause, end-stop)

			PatchParameter* target = modulator->mTarget.get();
			if (target == nullptr)
				continue;

			int count = target->getComponentCount();
			int from = modulator->mTargetComponent < 0 ? 0 : modulator->mTargetComponent;
			int to = modulator->mTargetComponent < 0 ? count - 1 : modulator->mTargetComponent;

			for (int s = 0; s < voices; ++s)
			{
				float v = modulator->valueForVoice(s);
				for (int c = from; c <= to && c < count; ++c)
				{
					float cur = target->getComponentValue(s, c);
					float blended = cur;
					switch (modulator->mBlend)
					{
					case EModulatorBlend::Replace:	blended = v;		break;
					case EModulatorBlend::Multiply:	blended = cur * v;	break;
					case EModulatorBlend::Add:		blended = cur + v;	break;
					}
					target->setComponentValue(s, c, blended);	// clamps 0..1
				}
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
