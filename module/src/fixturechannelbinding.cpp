#include "fixturechannelbinding.h"

#include <algorithm>

RTTI_BEGIN_CLASS(nap::FixtureChannelBinding)
	RTTI_PROPERTY("ChannelName",	&nap::FixtureChannelBinding::mChannelName,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("BaseParameter",	&nap::FixtureChannelBinding::mBaseParameter,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Overrides",		&nap::FixtureChannelBinding::mOverrides,		nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace nap
{
	bool FixtureChannelBinding::init(utility::ErrorState& errorState)
	{
		if (!errorState.check(mBaseParameter != nullptr, "%s: missing BaseParameter", mID.c_str()))
			return false;

		for (const auto& override_link : mOverrides)
		{
			if (!errorState.check(override_link->mLayer != nullptr, "%s: override missing Layer", mID.c_str()))
				return false;
			if (!errorState.check(override_link->mOverrideParameter != nullptr, "%s: override missing OverrideParameter", mID.c_str()))
				return false;
		}

		// Highest priority first
		std::sort(mOverrides.begin(), mOverrides.end(), [](const ResourcePtr<EffectLayerOverride>& a, const ResourcePtr<EffectLayerOverride>& b)
		{
			return a->mLayer->mPriority > b->mLayer->mPriority;
		});
		return true;
	}


	float FixtureChannelBinding::resolveValue() const
	{
		for (const auto& override_link : mOverrides)
		{
			if (override_link->mLayer->getIsActive())
				return override_link->mOverrideParameter->mValue;
		}
		return mBaseParameter->mValue;
	}


	void FixtureChannelBinding::addOverride(ResourcePtr<EffectLayerOverride> override)
	{
		mOverrides.emplace_back(override);
		std::sort(mOverrides.begin(), mOverrides.end(), [](const ResourcePtr<EffectLayerOverride>& a, const ResourcePtr<EffectLayerOverride>& b)
		{
			return a->mLayer->mPriority > b->mLayer->mPriority;
		});
	}
}
