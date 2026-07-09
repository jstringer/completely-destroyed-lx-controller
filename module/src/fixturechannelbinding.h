#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <parameternumeric.h>

// Local Includes
#include "effectlayeroverride.h"

namespace nap
{
	/**
	 * The full mixing configuration for one fixture channel: a base (static/MIDI/preset-driven)
	 * parameter, plus an ordered list of effect-layer overrides. resolveValue() returns the
	 * highest-priority active layer's override value, falling back to the base parameter.
	 */
	class NAPAPI FixtureChannelBinding : public Resource
	{
		RTTI_ENABLE(Resource)
	public:
		/**
		 * Sorts mOverrides by descending layer priority for fast per-frame resolution.
		 */
		virtual bool init(utility::ErrorState& errorState) override;

		/**
		 * @return the highest-priority active layer's override value, or the base parameter's value if no layer is active
		 */
		float resolveValue() const;

		/**
		 * Appends a runtime-created override link and re-sorts by descending layer priority.
		 * Used by lxcontrolService to attach a live-authored EffectLayer to an already-initialized
		 * (statically declared) FixtureChannelBinding without requiring a JSON rewrite.
		 * @param override the override link to add
		 */
		void addOverride(ResourcePtr<EffectLayerOverride> override);

		std::string								mChannelName;		///< Property: 'ChannelName' must match a FixtureChannel::mName
		ResourcePtr<ParameterFloat>				mBaseParameter;		///< Property: 'BaseParameter'
		std::vector<ResourcePtr<EffectLayerOverride>>	mOverrides;		///< Property: 'Overrides'
	};
}
