#pragma once

// External Includes
#include <nap/resource.h>
#include <string>
#include <vector>

namespace lx
{
	/**
	 * An ordered set of whole fixtures -- what a Control binds to. Order is meaningful: it sets the
	 * direction an effect spreads across the group (source 0 first), so reordering the members reverses
	 * a chase.
	 *
	 * Members are whole fixtures on purpose: a fixture's own sub-units (the Eurolite's six colour units)
	 * are declared at the fixture level and are abstracted away by the time it joins a group. The group's
	 * per-role source counts are derived from its members, never authored -- see
	 * lxcontrolService::sourcesFor / sourceCountsFor.
	 */
	class NAPAPI FixtureGroup : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		std::string					mName;			///< Property: 'Name'
		std::vector<std::string>	mFixtureNames;	///< Property: 'Fixtures' ordered fixture entity mIDs
	};
}
