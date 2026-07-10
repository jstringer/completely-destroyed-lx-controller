#include "fixturechannelcomponent.h"

#include <entity.h>
#include <mathutils.h>

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
		// ponytail: Phase 1 = base value only; LTP claim stack lands in Phase 3 with EffectParameter.
		return nap::math::clamp(mBaseParameter->mValue, 0.0f, 1.0f);
	}
}
