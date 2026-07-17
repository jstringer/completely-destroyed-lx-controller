#include "effect.h"

#include <algorithm>

RTTI_BEGIN_ENUM(lx::EEffectTargetMode)
	RTTI_ENUM_VALUE(lx::EEffectTargetMode::Single,		"Single"),
	RTTI_ENUM_VALUE(lx::EEffectTargetMode::Multiple,	"Multiple")
RTTI_END_ENUM

RTTI_BEGIN_CLASS(lx::Effect)
	RTTI_PROPERTY("Name",			&lx::Effect::mName,			nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Parameters",	&lx::Effect::mParameters,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Modulators",		&lx::Effect::mModulators,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("TargetMode",		&lx::Effect::mTargetMode,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("FixtureCount",	&lx::Effect::mFixtureCount,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace lx
{
	void Effect::trigger()
	{
		for (auto& modulator : mModulators)
			modulator->onTrigger();
	}


	void Effect::stop()
	{
		for (auto& modulator : mModulators)
			modulator->onStop();
	}


	void Effect::update(double deltaTime)
	{
		int slots = (mTargetMode == EEffectTargetMode::Multiple) ? std::max(1, mFixtureCount) : 1;

		for (auto& param : mParameters)
			param->resetToBase(slots);

		for (auto& modulator : mModulators)
		{
			modulator->update(deltaTime);	// per-frame transport housekeeping (sustain pause, end-stop)

			EffectParameter* target = modulator->mTarget.get();
			if (target == nullptr)
				continue;

			int count = target->getComponentCount();
			int from = modulator->mTargetComponent < 0 ? 0 : modulator->mTargetComponent;
			int to = modulator->mTargetComponent < 0 ? count - 1 : modulator->mTargetComponent;

			for (int s = 0; s < slots; ++s)
			{
				float v = modulator->valueForSlot(s);
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


	bool Effect::isFinished() const
	{
		for (auto& modulator : mModulators)
		{
			if (!modulator->isFinished())
				return false;
		}
		return true;
	}
}
