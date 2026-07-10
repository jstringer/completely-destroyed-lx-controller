#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <parameternumeric.h>

// Local Includes
#include "channelrole.h"

namespace lx
{
	/**
	 * Pure data describing what a fixture channel does: its semantic Role, an optional UnitIndex
	 * (1..6 for a unit-scoped RGB channel, 0 otherwise), and the base ParameterFloat that drives it.
	 * Written inline (Embedded) inside a FixtureChannelComponent.
	 */
	class NAPAPI FixtureChannelMapping : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		EChannelRole							mRole = EChannelRole::Generic;	///< Property: 'Role'
		int										mUnitIndex = 0;					///< Property: 'UnitIndex' 1..6 if unit-scoped, else 0
		nap::ResourcePtr<nap::ParameterFloat>	mBaseParameter;				///< Property: 'BaseParameter'
	};
}
