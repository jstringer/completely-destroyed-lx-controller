#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>

// Local Includes
#include "effectlayer.h"

namespace nap
{
	/**
	 * A named preset: a static fixture-value snapshot (blended to via a sibling
	 * ParameterBlendComponent, identified by mPresetFile) plus the set of effect layers
	 * that should be playing while this preset is active. Any EffectLayer known to the
	 * governing PresetManagerComponent that is not listed here is stopped when this preset
	 * is activated.
	 *
	 * Note: mPresetFile must correspond to a preset generated for the same ParameterBlendGroup
	 * used by the sibling ParameterBlendComponent, and the order of Presets in
	 * PresetManagerComponent::mPresets is expected to match the alphabetical preset-file order
	 * the ParameterBlendComponent sources from disk, since both are driven by the same
	 * 'ActivePresetIndex' parameter.
	 */
	class NAPAPI Preset : public Resource
	{
		RTTI_ENABLE(Resource)
	public:
		/**
		 * @param layer the layer to check
		 * @return whether this preset wants the given layer playing
		 */
		bool playsLayer(const EffectLayer& layer) const;

		std::string							mName;			///< Property: 'Name'
		std::string							mPresetFile;	///< Property: 'PresetFile' ParameterBlendComponent preset filename to blend to
		std::vector<ResourcePtr<EffectLayer>>	mLayersToPlay;	///< Property: 'LayersToPlay'
	};
}
