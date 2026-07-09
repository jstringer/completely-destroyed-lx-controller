#include "preset.h"

RTTI_BEGIN_CLASS(nap::Preset)
	RTTI_PROPERTY("Name",			&nap::Preset::mName,			nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("PresetFile",	&nap::Preset::mPresetFile,		nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("LayersToPlay",	&nap::Preset::mLayersToPlay,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("OnEnterEffect",	&nap::Preset::mOnEnterEffect,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("OnEnterAction",	&nap::Preset::mOnEnterAction,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("OnExitEffect",	&nap::Preset::mOnExitEffect,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("OnExitAction",	&nap::Preset::mOnExitAction,	nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("NoteMappings",	&nap::Preset::mNoteMappings,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace nap
{
	bool Preset::playsLayer(const EffectLayer& layer) const
	{
		for (const auto& target : mLayersToPlay)
		{
			if (target.get() == &layer)
				return true;
		}
		return false;
	}
}
