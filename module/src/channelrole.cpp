#include "channelrole.h"

#include <rtti/typeinfo.h>

RTTI_BEGIN_ENUM(lx::EChannelRole)
	RTTI_ENUM_VALUE(lx::EChannelRole::Dimmer,		"Dimmer"),
	RTTI_ENUM_VALUE(lx::EChannelRole::Strobe,		"Strobe"),
	RTTI_ENUM_VALUE(lx::EChannelRole::Red,			"Red"),
	RTTI_ENUM_VALUE(lx::EChannelRole::Green,		"Green"),
	RTTI_ENUM_VALUE(lx::EChannelRole::Blue,			"Blue"),
	RTTI_ENUM_VALUE(lx::EChannelRole::ColorMacro,	"ColorMacro"),
	RTTI_ENUM_VALUE(lx::EChannelRole::SoundMode,	"SoundMode"),
	RTTI_ENUM_VALUE(lx::EChannelRole::Generic,		"Generic")
RTTI_END_ENUM

RTTI_BEGIN_ENUM(lx::EDmxChannelWidth)
	RTTI_ENUM_VALUE(lx::EDmxChannelWidth::Value8,		"Value8"),
	RTTI_ENUM_VALUE(lx::EDmxChannelWidth::Value16MSB,	"Value16MSB"),
	RTTI_ENUM_VALUE(lx::EDmxChannelWidth::Value16LSB,	"Value16LSB")
RTTI_END_ENUM
