#pragma once

// External Includes
#include <component.h>
#include <nap/resourceptr.h>
#include <parameternumeric.h>
#include <cstdint>
#include <vector>

// Local Includes
#include "channelrole.h"
#include "fixturechannelmapping.h"
#include "effectparameter.h"

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
		 * @return this channel's current output value, 0..1: the highest-activation-id (latest-triggered,
		 * LTP) active claim's value, or the base parameter's value if no effect claims this channel.
		 */
		float resolveValue() const;

		/** Adds/replaces the claim for the given activation. Claims stay sorted ascending by id (latest last).
		 *  `slot` selects which fixture slot of `param` this claim reads (see Effect::mTargetMode); 0 for
		 *  Single-mode effects. */
		void pushClaim(uint64_t activationId, const EffectParameter* param, int component, int slot = 0);
		/** Removes any claim for the given activation. */
		void removeClaims(uint64_t activationId);

		int getOffset() const				{ return mOffset; }
		EChannelRole getRole() const		{ return mRole; }
		int getUnitIndex() const			{ return mUnitIndex; }
		const std::string& getChannelName() const	{ return mName; }
		size_t getClaimCount() const		{ return mClaims.size(); }

	private:
		struct ChannelClaim
		{
			uint64_t				mActivationId = 0;
			const EffectParameter*	mParam = nullptr;
			int						mComponent = 0;
			int						mSlot = 0;
		};

		std::string				mName;
		int						mOffset = 0;
		EChannelRole			mRole = EChannelRole::Generic;
		int						mUnitIndex = 0;
		nap::ParameterFloat*	mBaseParameter = nullptr;
		std::vector<ChannelClaim>	mClaims;	///< sorted ascending by activation id (latest-triggered last)
	};
}
