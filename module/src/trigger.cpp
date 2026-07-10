#include "trigger.h"

RTTI_BEGIN_STRUCT(lx::EffectFixtureBinding)
	RTTI_PROPERTY("Effect",		&lx::EffectFixtureBinding::mEffect,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Fixtures",	&lx::EffectFixtureBinding::mFixtureNames,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_STRUCT

RTTI_BEGIN_CLASS(lx::Trigger)
	RTTI_PROPERTY("Name",		&lx::Trigger::mName,		nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Bindings",	&lx::Trigger::mBindings,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

RTTI_BEGIN_CLASS(lx::ControllerTrigger)
RTTI_END_CLASS

RTTI_BEGIN_CLASS(lx::EnterTrigger)
RTTI_END_CLASS

RTTI_BEGIN_CLASS(lx::ExitTrigger)
RTTI_END_CLASS
