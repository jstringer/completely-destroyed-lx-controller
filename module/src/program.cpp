#include "program.h"

RTTI_BEGIN_CLASS(lx::Program)
	RTTI_PROPERTY("Name",					&lx::Program::mName,				nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("LifecycleTriggers",		&lx::Program::mLifecycleTriggers,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("ControlMappings",	&lx::Program::mControlMappings,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS
