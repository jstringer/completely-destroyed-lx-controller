#include "trigger.h"

RTTI_BEGIN_STRUCT(lx::PatchFixtureBinding)
	RTTI_PROPERTY("Patch",		&lx::PatchFixtureBinding::mPatch,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Groups",		&lx::PatchFixtureBinding::mGroups,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("EndToEnd",	&lx::PatchFixtureBinding::mEndToEnd,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_STRUCT

RTTI_BEGIN_ENUM(lx::ETriggerKind)
	RTTI_ENUM_VALUE(lx::ETriggerKind::Control,	"Control"),
	RTTI_ENUM_VALUE(lx::ETriggerKind::Enter,	"Enter"),
	RTTI_ENUM_VALUE(lx::ETriggerKind::Exit,		"Exit")
RTTI_END_ENUM

RTTI_BEGIN_CLASS(lx::Trigger)
	RTTI_PROPERTY("Name",		&lx::Trigger::mName,		nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Kind",		&lx::Trigger::mKind,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Bindings",	&lx::Trigger::mBindings,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS
