#pragma once

// External Includes
#include <component.h>
#include <nap/resourceptr.h>
#include <artnetcontroller.h>

namespace lx
{
	class FixtureComponentInstance;
	class FixtureChannelComponentInstance;

	/**
	 * A patched fixture: the ArtNetController/universe it lives on and its start channel. Its channels
	 * are sibling FixtureChannelComponents on the same entity. Each frame it writes every channel's
	 * resolved value out over Art-Net at (StartChannel + channel offset). On init it self-registers
	 * with lxcontrolService so effects/triggers/GUI can find it by entity mID.
	 */
	class NAPAPI FixtureComponent : public nap::Component
	{
		RTTI_ENABLE(nap::Component)
		DECLARE_COMPONENT(FixtureComponent, FixtureComponentInstance)
	public:
		FixtureComponent() : nap::Component() { }

		/**
		 * Declares a dependency on FixtureChannelComponent so all sibling channels are init'd before
		 * this fixture's init() runs — it reads their (instance-cached) offsets to validate the layout.
		 */
		virtual void getDependentComponents(std::vector<nap::rtti::TypeInfo>& components) const override;

		std::string								mDisplayName;		///< Property: 'DisplayName'
		nap::ResourcePtr<nap::ArtNetController>	mController;		///< Property: 'Controller' universe this fixture is patched into
		int										mStartChannel = 0;	///< Property: 'StartChannel' zero-based within the controller's channels
	};


	class NAPAPI FixtureComponentInstance : public nap::ComponentInstance
	{
		RTTI_ENABLE(nap::ComponentInstance)
	public:
		FixtureComponentInstance(nap::EntityInstance& entity, nap::Component& resource) :
			nap::ComponentInstance(entity, resource) { }

		virtual bool init(nap::utility::ErrorState& errorState) override;
		virtual void onDestroy() override;
		virtual void update(double deltaTime) override;

		const std::string& getDisplayName() const	{ return mDisplayName; }
		int getStartChannel() const					{ return mStartChannel; }
		const std::vector<FixtureChannelComponentInstance*>& getChannels() const { return mChannels; }

		/** @return the mID of the entity instance this fixture lives on (stable across runs; used to bind triggers). */
		std::string getEntityID() const;

	private:
		std::string								mDisplayName;
		int										mStartChannel = 0;
		nap::ArtNetController*					mController = nullptr;
		std::vector<FixtureChannelComponentInstance*>	mChannels;
	};
}
