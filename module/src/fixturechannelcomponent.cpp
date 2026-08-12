#include "fixturechannelcomponent.h"

#include <entity.h>
#include <mathutils.h>
#include <algorithm>

RTTI_BEGIN_CLASS(lx::FixtureChannelComponent)
	RTTI_PROPERTY("Name",			&lx::FixtureChannelComponent::mName,			nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Offset",			&lx::FixtureChannelComponent::mOffset,			nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Role",			&lx::FixtureChannelComponent::mRole,			nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("UnitIndex",		&lx::FixtureChannelComponent::mUnitIndex,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("BaseParameter",	&lx::FixtureChannelComponent::mBaseParameter,	nap::rtti::EPropertyMetaData::Required)
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
		mRole = resource->mRole;
		mUnitIndex = resource->mUnitIndex;
		mBaseParameter = resource->mBaseParameter.get();
		return errorState.check(mBaseParameter != nullptr, "%s: missing BaseParameter", mID.c_str());
	}


	float FixtureChannelComponentInstance::resolveValue() const
	{
		// Held-priority LTP: prefer the newest claim from a currently-held control; if none is held, fall
		// back to the newest remaining claim (a stab / released gesture ringing out); else the base value.
		// mClaims is sorted ascending by activation id, so the last match in the scan is the newest.
		const ChannelClaim* winner = nullptr;
		for (const ChannelClaim& c : mClaims)
		{
			if (c.mHeld)
				winner = &c;			// newest held so far
		}
		if (winner == nullptr && !mClaims.empty())
			winner = &mClaims.back();	// nothing held -> newest overall rings out
		if (winner != nullptr && winner->mPatch != nullptr && winner->mParam != nullptr)
		{
			// Evaluated here, on demand: the claim knows where this channel sits in the fired group's source
			// list, so no per-source buffer has to be kept in sync anywhere.
			// ponytail: ~66 evaluate() calls/frame on this rig (3 fixtures x 22 channels), each looping a
			// handful of modulators. Memoize per (patch, sourceIndex) only if a profile ever says so.
			return nap::math::clamp(winner->mPatch->evaluate(*winner->mParam, winner->mSourceIndex,
				winner->mSourceCount, winner->mComponent), 0.0f, 1.0f);
		}
		return nap::math::clamp(mBaseParameter->mValue, 0.0f, 1.0f);
	}


	void FixtureChannelComponentInstance::pushClaim(uint64_t activationId, const Patch* patch,
		const PatchParameter* param, int component, int sourceIndex, int sourceCount, bool held)
	{
		removeClaims(activationId);
		// Activation ids are monotonically increasing, so a new claim is always the latest -> append
		// keeps the vector sorted ascending by id.
		mClaims.push_back({ activationId, patch, param, component, sourceIndex, sourceCount, held });
	}


	void FixtureChannelComponentInstance::removeClaims(uint64_t activationId)
	{
		mClaims.erase(std::remove_if(mClaims.begin(), mClaims.end(),
			[activationId](const ChannelClaim& c) { return c.mActivationId == activationId; }), mClaims.end());
	}


	void FixtureChannelComponentInstance::releaseClaims(uint64_t activationId)
	{
		for (ChannelClaim& c : mClaims)
			if (c.mActivationId == activationId)
				c.mHeld = false;
	}
}
