#include "effectlayer.h"

RTTI_BEGIN_CLASS(nap::EffectLayer)
	RTTI_PROPERTY("Name",		&nap::EffectLayer::mName,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Player",	&nap::EffectLayer::mPlayer,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Priority",	&nap::EffectLayer::mPriority,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS
