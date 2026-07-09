#include "effectlayeroverride.h"
#include "fixturechannelbinding.h"

RTTI_BEGIN_CLASS(nap::EffectLayerOverride)
	RTTI_PROPERTY("Layer",				&nap::EffectLayerOverride::mLayer,				nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("OverrideParameter",	&nap::EffectLayerOverride::mOverrideParameter,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("TargetBinding",		&nap::EffectLayerOverride::mTargetBinding,		nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS
