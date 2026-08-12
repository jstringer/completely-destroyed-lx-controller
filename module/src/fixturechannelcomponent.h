#pragma once

// External Includes
#include <component.h>
#include <nap/resourceptr.h>
#include <parameternumeric.h>
#include <cstdint>
#include <vector>

// Local Includes
#include "channelrole.h"
#include "patch.h"
#include "patchparameter.h"

namespace lx
{
	class FixtureChannelComponentInstance;

	/**
	 * One DMX channel of a fixture: a name, an offset within the fixture's channel block, what the
	 * channel does (semantic Role + an optional UnitIndex, 1..6 for a unit-scoped RGB channel), and the
	 * base ParameterFloat that drives it. Sits as a sibling component alongside a FixtureComponent on a
	 * fixture entity. Every channel is one 8-bit DMX slot.
	 */
	class NAPAPI FixtureChannelComponent : public nap::Component
	{
		RTTI_ENABLE(nap::Component)
		DECLARE_COMPONENT(FixtureChannelComponent, FixtureChannelComponentInstance)
	public:
		FixtureChannelComponent() : nap::Component() { }

		std::string								mName;			///< Property: 'Name'
		int										mOffset = 0;	///< Property: 'Offset' zero-based within the fixture channel block
		EChannelRole							mRole = EChannelRole::Generic;	///< Property: 'Role'
		int										mUnitIndex = 0;	///< Property: 'UnitIndex' 1..6 if unit-scoped, else 0
		nap::ResourcePtr<nap::ParameterFloat>	mBaseParameter;	///< Property: 'BaseParameter'
	};


	class NAPAPI FixtureChannelComponentInstance : public nap::ComponentInstance
	{
		RTTI_ENABLE(nap::ComponentInstance)
	public:
		FixtureChannelComponentInstance(nap::EntityInstance& entity, nap::Component& resource) :
			nap::ComponentInstance(entity, resource) { }

		virtual bool init(nap::utility::ErrorState& errorState) override;

		/**
		 * @return this channel's current output value, 0..1. Held-priority LTP: the newest claim fired by a
		 * currently-held control (Momentary down / Latch on) wins; if none is held, the newest remaining
		 * (stab / released gesture) claim wins so it rings out; else the base parameter value.
		 */
		float resolveValue() const;

		/** Adds/replaces the claim for the given activation. Claims stay sorted ascending by id (latest last).
		 *  `sourceIndex`/`sourceCount` locate this channel in the fired group's source list; the value itself
		 *  is evaluated on demand in resolveValue(), so no per-source buffer exists anywhere.
		 *  `held` marks a claim from a currently-held control so it outranks stabs. */
		void pushClaim(uint64_t activationId, const Patch* patch, const PatchParameter* param, int component,
			int sourceIndex, int sourceCount, bool held);
		/** Removes any claim for the given activation. */
		void removeClaims(uint64_t activationId);
		/** Marks this activation's claims as no longer held (its control was released) so a still-held lower
		 *  claim reclaims the channel; the released claim rings out only where nothing held remains. */
		void releaseClaims(uint64_t activationId);

		int getOffset() const				{ return mOffset; }
		EChannelRole getRole() const		{ return mRole; }
		int getUnitIndex() const			{ return mUnitIndex; }
		const std::string& getChannelName() const	{ return mName; }

	private:
		struct ChannelClaim
		{
			uint64_t				mActivationId = 0;
			const Patch*			mPatch = nullptr;		// evaluates the value; owns the modulators
			const PatchParameter*	mParam = nullptr;
			int						mComponent = 0;
			int						mSourceIndex = 0;		// where this channel sits in the fired source list
			int						mSourceCount = 1;
			bool					mHeld = false;	// fired by a currently-held control (Momentary down / Latch on) -> outranks stabs/releasing
		};

		std::string				mName;
		int						mOffset = 0;
		EChannelRole			mRole = EChannelRole::Generic;
		int						mUnitIndex = 0;
		nap::ParameterFloat*	mBaseParameter = nullptr;
		std::vector<ChannelClaim>	mClaims;	///< sorted ascending by activation id (latest-triggered last)
	};
}
