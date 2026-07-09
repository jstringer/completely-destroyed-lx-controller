#include "fixturechannel.h"

RTTI_BEGIN_ENUM(nap::EDmxChannelType)
	RTTI_ENUM_VALUE(nap::EDmxChannelType::Value8,		"Value8"),
	RTTI_ENUM_VALUE(nap::EDmxChannelType::Value16MSB,	"Value16MSB"),
	RTTI_ENUM_VALUE(nap::EDmxChannelType::Value16LSB,	"Value16LSB")
RTTI_END_ENUM

RTTI_BEGIN_CLASS(nap::FixtureChannel)
	RTTI_PROPERTY("Name",	&nap::FixtureChannel::mName,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Offset",	&nap::FixtureChannel::mOffset,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("ChannelType",	&nap::FixtureChannel::mType,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS
