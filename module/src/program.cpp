#include "program.h"

RTTI_BEGIN_CLASS(lx::Program)
	RTTI_PROPERTY("Name",		&lx::Program::mName,		nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Triggers",	&lx::Program::mTriggers,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS
