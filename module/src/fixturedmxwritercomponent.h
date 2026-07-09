#pragma once

// External Includes
#include <component.h>

// Local Includes
#include "fixture.h"

namespace nap
{
	class FixtureDmxWriterComponentInstance;

	/**
	 * Reads a Fixture's resolved channel values (each already mixed by FixtureChannelBinding)
	 * every frame and writes them out over Art-Net via the Fixture's ArtNetController.
	 */
	class NAPAPI FixtureDmxWriterComponent : public Component
	{
		RTTI_ENABLE(Component)
		DECLARE_COMPONENT(FixtureDmxWriterComponent, FixtureDmxWriterComponentInstance)
	public:
		FixtureDmxWriterComponent() : Component() { }

		ResourcePtr<Fixture> mFixture;		///< Property: 'Fixture'
	};


	class NAPAPI FixtureDmxWriterComponentInstance : public ComponentInstance
	{
		RTTI_ENABLE(ComponentInstance)
	public:
		FixtureDmxWriterComponentInstance(EntityInstance& entity, Component& resource) :
			ComponentInstance(entity, resource) { }

		virtual bool init(utility::ErrorState& errorState) override;
		virtual void update(double deltaTime) override;

	private:
		Fixture* mFixture = nullptr;
	};
}
