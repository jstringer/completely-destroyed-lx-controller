#include "fixturechannelcomponent.h"

#include <entity.h>
#include <mathutils.h>
#include <algorithm>

RTTI_BEGIN_CLASS(lx::FixtureChannelComponent)
	RTTI_PROPERTY("Name",		&lx::FixtureChannelComponent::mName,		nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Offset",		&lx::FixtureChannelComponent::mOffset,		nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Width",		&lx::FixtureChannelComponent::mWidth,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Mapping",	&lx::FixtureChannelComponent::mMapping,		nap::rtti::EPropertyMetaData::Required | nap::rtti::EPropertyMetaData::Embedded)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(lx::FixtureChannelComponentInstance)
	RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)
RTTI_END_CLASS

namespace lx
{
	bool FixtureChannelComponentInstance::init(nap::utility::ErrorState& errorState)
	{
		auto* resource = getComponent<FixtureChannelComponent>();
		mName = resource->mName;
		mOffset = resource->mOffset;

		if (!errorState.check(resource->mMapping != nullptr, "%s: missing Mapping", mID.c_str()))
			return false;

		mRole = resource->mMapping->mRole;
		mUnitIndex = resource->mMapping->mUnitIndex;
		mBaseParameter = resource->mMapping->mBaseParameter.get();
		return errorState.check(mBaseParameter != nullptr, "%s: mapping missing BaseParameter", mID.c_str());
	}


	float FixtureChannelComponentInstance::resolveValue() const
	{
		// LTP: latest-triggered active claim (largest activation id, kept last) wins; else base value.
		if (!mClaims.empty())
		{
			const ChannelClaim& top = mClaims.back();
			return nap::math::clamp(top.mParam->getComponentValue(top.mComponent), 0.0f, 1.0f);
		}
		return nap::math::clamp(mBaseParameter->mValue, 0.0f, 1.0f);
	}


	void FixtureChannelComponentInstance::pushClaim(uint64_t activationId, const EffectParameter* param, int component)
	{
		removeClaims(activationId);
		// Activation ids are monotonically increasing, so a new claim is always the latest -> append
		// keeps the vector sorted ascending by id.
		mClaims.push_back({ activationId, param, component });
	}


	void FixtureChannelComponentInstance::removeClaims(uint64_t activationId)
	{
		mClaims.erase(std::remove_if(mClaims.begin(), mClaims.end(),
			[activationId](const ChannelClaim& c) { return c.mActivationId == activationId; }), mClaims.end());
	}
}
