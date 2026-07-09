#include "fixturedmxwritercomponent.h"

#include <entity.h>

RTTI_BEGIN_CLASS(nap::FixtureDmxWriterComponent)
	RTTI_PROPERTY("Fixture", &nap::FixtureDmxWriterComponent::mFixture, nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::FixtureDmxWriterComponentInstance)
	RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)
RTTI_END_CLASS

namespace nap
{
	bool FixtureDmxWriterComponentInstance::init(utility::ErrorState& errorState)
	{
		auto* resource = getComponent<FixtureDmxWriterComponent>();
		mFixture = resource->mFixture.get();
		return errorState.check(mFixture != nullptr, "%s: missing Fixture", mID.c_str());
	}


	void FixtureDmxWriterComponentInstance::update(double deltaTime)
	{
		for (const auto& resolved : mFixture->mResolvedChannels)
		{
			float value = resolved.mBinding->resolveValue();
			mFixture->mController->send(value, resolved.mAbsoluteChannel);
		}
	}
}
