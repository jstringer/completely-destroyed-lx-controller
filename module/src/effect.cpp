#include "effect.h"

RTTI_BEGIN_CLASS(lx::Effect)
	RTTI_PROPERTY("Name",		&lx::Effect::mName,			nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Parameters",	&lx::Effect::mParameters,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Modulators",	&lx::Effect::mModulators,	nap::rtti::EPropertyMetaData::Default)
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
		for (auto& param : mParameters)
			param->resetToBase();

		for (auto& modulator : mModulators)
		{
			modulator->advance(deltaTime);	// advance the modulator's own monotonic clock every frame

			EffectParameter* target = modulator->mTarget.get();
			if (target == nullptr)
				continue;

			float v = modulator->value();
			int count = target->getComponentCount();
			int from = modulator->mTargetComponent < 0 ? 0 : modulator->mTargetComponent;
			int to = modulator->mTargetComponent < 0 ? count - 1 : modulator->mTargetComponent;

			for (int c = from; c <= to && c < count; ++c)
			{
				float cur = target->getComponentValue(c);
				float blended = cur;
				switch (modulator->mBlend)
				{
				case EModulatorBlend::Replace:	blended = v;		break;
				case EModulatorBlend::Multiply:	blended = cur * v;	break;
				case EModulatorBlend::Add:		blended = cur + v;	break;
				}
				target->setComponentValue(c, blended);	// clamps 0..1
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
