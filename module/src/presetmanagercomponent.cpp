#include "presetmanagercomponent.h"

#include <entity.h>
#include <parameterblendcomponent.h>

RTTI_BEGIN_CLASS(nap::PresetManagerComponent)
	RTTI_PROPERTY("Presets",			&nap::PresetManagerComponent::mPresets,				nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("AllLayers",			&nap::PresetManagerComponent::mAllLayers,				nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("ActivePresetIndex",	&nap::PresetManagerComponent::mActivePresetIndex,		nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::PresetManagerComponentInstance)
	RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)
RTTI_END_CLASS

namespace nap
{
	void PresetManagerComponent::getDependentComponents(std::vector<rtti::TypeInfo>& components) const
	{
		components.emplace_back(RTTI_OF(ParameterBlendComponent));
	}


	bool PresetManagerComponentInstance::init(utility::ErrorState& errorState)
	{
		auto* resource = getComponent<PresetManagerComponent>();

		mActivePresetIndex = resource->mActivePresetIndex.get();
		if (!errorState.check(mActivePresetIndex != nullptr, "%s: missing ActivePresetIndex", mID.c_str()))
			return false;

		if (!errorState.check(getEntityInstance()->findComponent<ParameterBlendComponentInstance>() != nullptr,
			"%s: missing sibling ParameterBlendComponent", mID.c_str()))
			return false;

		for (const auto& preset : resource->mPresets)
			mPresets.emplace_back(preset.get());

		for (const auto& layer : resource->mAllLayers)
			mAllLayers.emplace_back(layer.get());

		mActivePresetIndex->valueChanged.connect(mIndexChangedSlot);
		return true;
	}


	void PresetManagerComponentInstance::onPresetIndexChanged(int index)
	{
		if (index < 0 || index >= static_cast<int>(mPresets.size()))
			return;

		Preset* preset = mPresets[index];
		for (auto* layer : mAllLayers)
			layer->mPlayer->setIsPlaying(preset->playsLayer(*layer));
	}
}
