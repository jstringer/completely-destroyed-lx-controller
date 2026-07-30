#include "control.h"

RTTI_BEGIN_ENUM(lx::EControlMode)
	RTTI_ENUM_VALUE(lx::EControlMode::Momentary,	"Momentary"),
	RTTI_ENUM_VALUE(lx::EControlMode::Toggle,	"Toggle"),
	RTTI_ENUM_VALUE(lx::EControlMode::FireOnly,	"FireOnly")
RTTI_END_ENUM

RTTI_BEGIN_ENUM(lx::EControlKind)
	RTTI_ENUM_VALUE(lx::EControlKind::Pad,	"Pad"),
	RTTI_ENUM_VALUE(lx::EControlKind::Knob,	"Knob")
RTTI_END_ENUM

RTTI_BEGIN_CLASS(lx::Control)
	RTTI_PROPERTY("Name",		&lx::Control::mName,		nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Mode",		&lx::Control::mMode,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Kind",		&lx::Control::mKind,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Group",		&lx::Control::mGroup,		nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS
