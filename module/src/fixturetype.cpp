#include "fixturetype.h"

RTTI_BEGIN_CLASS(nap::FixtureType)
	RTTI_PROPERTY("Channels", &nap::FixtureType::mChannels, nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace nap
{
	bool FixtureType::init(utility::ErrorState& errorState)
	{
		for (int i = 0; i < mChannels.size(); i++)
		{
			for (int j = i + 1; j < mChannels.size(); j++)
			{
				if (!errorState.check(mChannels[i]->mName != mChannels[j]->mName,
					"%s: duplicate channel name '%s'", mID.c_str(), mChannels[i]->mName.c_str()))
					return false;

				if (!errorState.check(mChannels[i]->mOffset != mChannels[j]->mOffset,
					"%s: duplicate channel offset %d ('%s' and '%s')", mID.c_str(), mChannels[i]->mOffset,
					mChannels[i]->mName.c_str(), mChannels[j]->mName.c_str()))
					return false;
			}
		}
		return true;
	}


	const FixtureChannel* FixtureType::findChannel(const std::string& name) const
	{
		for (const auto& channel : mChannels)
		{
			if (channel->mName == name)
				return channel.get();
		}
		return nullptr;
	}
}
