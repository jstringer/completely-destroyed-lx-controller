#include "controlmapping.h"

RTTI_BEGIN_CLASS(lx::ControlMapping)
	RTTI_PROPERTY("Control",	&lx::ControlMapping::mControl,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Trigger",	&lx::ControlMapping::mTrigger,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS
