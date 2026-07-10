#include "fixturecomponent.h"
#include "fixturechannelcomponent.h"
#include "lxcontrolservice.h"

#include <entity.h>
#include <nap/core.h>
#include <set>

RTTI_BEGIN_CLASS(lx::FixtureComponent)
	RTTI_PROPERTY("DisplayName",	&lx::FixtureComponent::mDisplayName,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Controller",		&lx::FixtureComponent::mController,		nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("StartChannel",	&lx::FixtureComponent::mStartChannel,	nap::rtti::EPropertyMetaData::Required)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(lx::FixtureComponentInstance)
	RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)
RTTI_END_CLASS

namespace lx
{
	void FixtureComponent::getDependentComponents(std::vector<nap::rtti::TypeInfo>& components) const
	{
		components.push_back(RTTI_OF(lx::FixtureChannelComponent));
	}


	bool FixtureComponentInstance::init(nap::utility::ErrorState& errorState)
	{
		auto* resource = getComponent<FixtureComponent>();
		mDisplayName = resource->mDisplayName;
		mStartChannel = resource->mStartChannel;
		mController = resource->mController.get();
		if (!errorState.check(mController != nullptr, "%s: missing Controller", mID.c_str()))
			return false;

		// Gather sibling channels (read-only). Their own resource props are available regardless of
		// init order, so no getDependentComponents() dependency is needed.
		getEntityInstance()->getComponentsOfType<FixtureChannelComponentInstance>(mChannels);
		if (!errorState.check(!mChannels.empty(), "%s: fixture has no channels", mID.c_str()))
			return false;

		// Validate no two channels resolve to the same absolute DMX slot (Value8 = one slot each).
		std::set<int> used;
		for (auto* channel : mChannels)
		{
			int abs_channel = mStartChannel + channel->getOffset();
			if (!errorState.check(used.insert(abs_channel).second,
				"%s: channel offset collision at absolute channel %d", mID.c_str(), abs_channel))
				return false;
		}

		auto* service = getEntityInstance()->getCore()->getService<nap::lxcontrolService>();
		if (service != nullptr)
			service->registerFixture(this);
		return true;
	}


	void FixtureComponentInstance::onDestroy()
	{
		auto* service = getEntityInstance()->getCore()->getService<nap::lxcontrolService>();
		if (service != nullptr)
			service->unregisterFixture(this);
	}


	void FixtureComponentInstance::update(double deltaTime)
	{
		// ponytail: absolute channel = StartChannel + Offset at the point of use; not cached.
		for (auto* channel : mChannels)
			mController->send(channel->resolveValue(), mStartChannel + channel->getOffset());
	}


	std::string FixtureComponentInstance::getEntityID() const
	{
		return getEntityInstance()->mID;
	}
}
