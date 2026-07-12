#include "controller.h"

RTTI_BEGIN_ENUM(lx::EControllerMode)
	RTTI_ENUM_VALUE(lx::EControllerMode::Momentary,	"Momentary"),
	RTTI_ENUM_VALUE(lx::EControllerMode::Toggle,	"Toggle"),
	RTTI_ENUM_VALUE(lx::EControllerMode::FireOnly,	"FireOnly")
RTTI_END_ENUM

RTTI_BEGIN_CLASS(lx::Controller)
	RTTI_PROPERTY("Name",		&lx::Controller::mName,		nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Mode",		&lx::Controller::mMode,		nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS
