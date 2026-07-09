#include "presetmiditrigger.h"

RTTI_BEGIN_CLASS(nap::PresetMidiTrigger)
	RTTI_PROPERTY("Number",		&nap::PresetMidiTrigger::mNumber,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("EffectLayer",	&nap::PresetMidiTrigger::mEffectLayer,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Action",		&nap::PresetMidiTrigger::mAction,		nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace nap
{
	bool PresetMidiTrigger::init(utility::ErrorState& errorState)
	{
		return errorState.check(mEffectLayer != nullptr, "%s: PresetMidiTrigger requires an EffectLayer", mID.c_str());
	}
}
