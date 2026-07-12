#include "controllermapping.h"

RTTI_BEGIN_CLASS(lx::ControllerMapping)
	RTTI_PROPERTY("Controller",	&lx::ControllerMapping::mController,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Trigger",	&lx::ControllerMapping::mTrigger,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS
