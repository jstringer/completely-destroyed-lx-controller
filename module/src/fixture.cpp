#include "fixture.h"

#include <algorithm>

RTTI_BEGIN_CLASS(nap::Fixture)
	RTTI_PROPERTY("FixtureType",		&nap::Fixture::mFixtureType,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Controller",		&nap::Fixture::mController,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("StartChannel",		&nap::Fixture::mStartChannel,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("ChannelBindings",	&nap::Fixture::mChannelBindings,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace nap
{
	bool Fixture::init(utility::ErrorState& errorState)
	{
		if (!errorState.check(mFixtureType != nullptr, "%s: missing FixtureType", mID.c_str()))
			return false;

		if (!errorState.check(mController != nullptr, "%s: missing Controller", mID.c_str()))
			return false;

		for (const auto& channel : mFixtureType->mChannels)
		{
			auto it = std::find_if(mChannelBindings.begin(), mChannelBindings.end(), [&](const ResourcePtr<FixtureChannelBinding>& binding)
			{
				return binding->mChannelName == channel->mName;
			});

			if (!errorState.check(it != mChannelBindings.end(),
				"%s: no ChannelBinding found for channel '%s'", mID.c_str(), channel->mName.c_str()))
				return false;

			ResolvedChannel resolved;
			resolved.mAbsoluteChannel = mStartChannel + channel->mOffset;
			resolved.mBinding = (*it).get();
			mResolvedChannels.emplace_back(resolved);
		}
		return true;
	}
}
