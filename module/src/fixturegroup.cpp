#include "fixturegroup.h"

RTTI_BEGIN_CLASS(lx::FixtureGroup)
	RTTI_PROPERTY("Name",		&lx::FixtureGroup::mName,			nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Fixtures",	&lx::FixtureGroup::mFixtureNames,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS
