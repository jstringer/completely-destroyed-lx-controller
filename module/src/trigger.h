#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <vector>
#include <string>

// Local Includes
#include "effect.h"

namespace lx
{
	/**
	 * Binds an Effect to a set of fixtures (by entity mID). When the owning Trigger fires, the effect
	 * is triggered and claims the matching channels on each named fixture.
	 */
	struct NAPAPI EffectFixtureBinding
	{
		nap::ResourcePtr<Effect>	mEffect;			///< Property: 'Effect'
		std::vector<std::string>	mFixtureNames;		///< Property: 'Fixtures' fixture entity mIDs
	};


	/**
	 * A named set of effect->fixture bindings that can be fired/stopped. Firing is arbitrated by
	 * lxcontrolService (which owns activation ids + the LTP claim stack). Subtypes only differ in how
	 * they are fired: a ControllerTrigger by a MIDI-mapped Controller, Enter/Exit by Program load/unload.
	 */
	class NAPAPI Trigger : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		std::string							mName;			///< Property: 'Name'
		std::vector<EffectFixtureBinding>	mBindings;		///< Property: 'Bindings'
	};

	class NAPAPI ControllerTrigger : public Trigger	{ RTTI_ENABLE(Trigger) };
	class NAPAPI EnterTrigger : public Trigger		{ RTTI_ENABLE(Trigger) };
	class NAPAPI ExitTrigger : public Trigger		{ RTTI_ENABLE(Trigger) };
}
