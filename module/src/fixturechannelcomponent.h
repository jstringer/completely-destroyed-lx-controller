#pragma once

// External Includes
#include <component.h>
#include <nap/resourceptr.h>
#include <parameternumeric.h>

// Local Includes
#include "channelrole.h"
#include "fixturechannelmapping.h"

namespace lx
{
	class FixtureChannelComponentInstance;

	/**
	 * One DMX channel of a fixture: a name, an offset within the fixture's channel block, a DMX width,
	 * and an embedded FixtureChannelMapping (role + base parameter). Sits as a sibling component
	 * alongside a FixtureComponent on a fixture entity.
	 */
	class NAPAPI FixtureChannelComponent : public nap::Component
	{
		RTTI_ENABLE(nap::Component)
		DECLARE_COMPONENT(FixtureChannelComponent, FixtureChannelComponentInstance)
	public:
		FixtureChannelComponent() : nap::Component() { }

		std::string								mName;								///< Property: 'Name'
		int										mOffset = 0;						///< Property: 'Offset' zero-based within the fixture channel block
		EDmxChannelWidth						mWidth = EDmxChannelWidth::Value8;	///< Property: 'Width'
		nap::ResourcePtr<FixtureChannelMapping>	mMapping;						///< Property: 'Mapping' (embedded)
	};


	class NAPAPI FixtureChannelComponentInstance : public nap::ComponentInstance
	{
		RTTI_ENABLE(nap::ComponentInstance)
	public:
		FixtureChannelComponentInstance(nap::EntityInstance& entity, nap::Component& resource) :
			nap::ComponentInstance(entity, resource) { }

		virtual bool init(nap::utility::ErrorState& errorState) override;

		/**
		 * @return this channel's current output value, 0..1.
		 * Phase 1: the base parameter's value. The LTP claim stack (highest-recent active effect
		 * claim wins, else base) is added in Phase 3 with EffectParameter.
		 */
		float resolveValue() const;

		int getOffset() const				{ return mOffset; }
		EChannelRole getRole() const		{ return mRole; }
		int getUnitIndex() const			{ return mUnitIndex; }
		const std::string& getChannelName() const	{ return mName; }

	private:
		std::string				mName;
		int						mOffset = 0;
		EChannelRole			mRole = EChannelRole::Generic;
		int						mUnitIndex = 0;
		nap::ParameterFloat*	mBaseParameter = nullptr;
	};
}
