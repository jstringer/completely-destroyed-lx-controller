#pragma once

// External Includes
#include <component.h>
#include <parameternumeric.h>

// Local Includes
#include "preset.h"

namespace nap
{
	class PresetManagerComponentInstance;

	/**
	 * Selects the active Preset (via 'ActivePresetIndex') and starts/stops the EffectLayers
	 * it lists. Must be declared alongside a sibling nap::ParameterBlendComponent whose
	 * 'PresetIndex' is the same Parameter as this component's 'ActivePresetIndex' -- the blend
	 * component crossfades static fixture values on its own when that shared parameter changes,
	 * this component only needs to start/stop effect layers in response.
	 */
	class NAPAPI PresetManagerComponent : public Component
	{
		RTTI_ENABLE(Component)
		DECLARE_COMPONENT(PresetManagerComponent, PresetManagerComponentInstance)
	public:
		PresetManagerComponent() : Component() { }

		/**
		 * Depends on the sibling ParameterBlendComponent so it initializes first.
		 */
		virtual void getDependentComponents(std::vector<rtti::TypeInfo>& components) const override;

		std::vector<ResourcePtr<Preset>>		mPresets;				///< Property: 'Presets'
		std::vector<ResourcePtr<EffectLayer>>	mAllLayers;				///< Property: 'AllLayers' universe of layers this manager governs
		ResourcePtr<ParameterInt>				mActivePresetIndex;		///< Property: 'ActivePresetIndex'
	};


	class NAPAPI PresetManagerComponentInstance : public ComponentInstance
	{
		RTTI_ENABLE(ComponentInstance)
	public:
		PresetManagerComponentInstance(EntityInstance& entity, Component& resource) :
			ComponentInstance(entity, resource) { }

		virtual bool init(utility::ErrorState& errorState) override;

	private:
		void onPresetIndexChanged(int index);
		Slot<int> mIndexChangedSlot = { this, &PresetManagerComponentInstance::onPresetIndexChanged };

		std::vector<Preset*> mPresets;
		std::vector<EffectLayer*> mAllLayers;
		ParameterInt* mActivePresetIndex = nullptr;
	};
}
