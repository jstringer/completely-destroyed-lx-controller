#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>

// Local Includes
#include "effectlayer.h"
#include "presetmiditrigger.h"
#include "miditriggercomponent.h"

namespace nap
{
	/**
	 * A named preset: a fixture-value snapshot (mPresetFile, one file per fixture, see
	 * lxcontrolService) plus the set of effect layers that should be playing while this preset
	 * is active. Any EffectLayer known to lxcontrolService that is not listed in mLayersToPlay
	 * is stopped when this preset is activated.
	 *
	 * A Preset also owns its own list of MIDI note->effect triggers (mNoteMappings), only
	 * consulted while this preset is the currently-active one, and optional one-shot effect
	 * triggers fired when the preset becomes active (mOnEnterEffect) or stops being active
	 * (mOnExitEffect) - see lxcontrolService::applyPreset()/onMidiEvent().
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
		std::string							mPresetFile;	///< Property: 'PresetFile' per-fixture snapshot filename base
		std::vector<ResourcePtr<EffectLayer>>	mLayersToPlay;	///< Property: 'LayersToPlay'

		ResourcePtr<EffectLayer>			mOnEnterEffect;								///< Property: 'OnEnterEffect' optional, fired once when this preset becomes active
		EMidiTriggerAction					mOnEnterAction = EMidiTriggerAction::Play;	///< Property: 'OnEnterAction'
		ResourcePtr<EffectLayer>			mOnExitEffect;								///< Property: 'OnExitEffect' optional, fired once when this preset stops being active
		EMidiTriggerAction					mOnExitAction = EMidiTriggerAction::Stop;	///< Property: 'OnExitAction'

		std::vector<ResourcePtr<PresetMidiTrigger>>	mNoteMappings;	///< Property: 'NoteMappings' this preset's own note->effect triggers
	};
}
