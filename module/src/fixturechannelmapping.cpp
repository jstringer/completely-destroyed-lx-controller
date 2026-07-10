#include "fixturechannelmapping.h"

RTTI_BEGIN_CLASS(lx::FixtureChannelMapping)
	RTTI_PROPERTY("Role",			&lx::FixtureChannelMapping::mRole,			nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("UnitIndex",		&lx::FixtureChannelMapping::mUnitIndex,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("BaseParameter",	&lx::FixtureChannelMapping::mBaseParameter,	nap::rtti::EPropertyMetaData::Required)
RTTI_END_CLASS
