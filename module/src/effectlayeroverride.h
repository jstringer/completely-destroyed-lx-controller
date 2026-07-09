#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <parameternumeric.h>

// Local Includes
#include "effectlayer.h"

namespace nap
{
	class FixtureChannelBinding;

	/**
	 * Pairs an EffectLayer with the dedicated parameter its SequencePlayerCurveOutput
	 * writes to for one specific fixture channel. Used by FixtureChannelBinding to
	 * resolve which value wins when multiple layers are active simultaneously.
	 */
	class NAPAPI EffectLayerOverride : public Resource
	{
		RTTI_ENABLE(Resource)
	public:
		ResourcePtr<EffectLayer>				mLayer;					///< Property: 'Layer'
		ResourcePtr<ParameterFloat>				mOverrideParameter;		///< Property: 'OverrideParameter'

		/**
		 * The FixtureChannelBinding this override was runtime-attached to (via addOverride()).
		 * Not used for statically JSON-declared overrides; only recorded so lxcontrolService can
		 * replay the addOverride() call after reloading this object from user_content.json at startup.
		 */
		ResourcePtr<FixtureChannelBinding>		mTargetBinding;			///< Property: 'TargetBinding'
	};
}
