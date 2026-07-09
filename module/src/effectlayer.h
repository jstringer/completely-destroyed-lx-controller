#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <sequenceplayer.h>

namespace nap
{
	/**
	 * A named, prioritized effect layer: a SequencePlayer that can be started/stopped
	 * (from a Preset or a MidiTriggerComponent) and whose curve tracks drive per-fixture-channel
	 * override parameters (see EffectLayerOverride). Multiple layers can run concurrently;
	 * FixtureChannelBinding resolves conflicts using mPriority (highest active layer wins).
	 */
	class NAPAPI EffectLayer : public Resource
	{
		RTTI_ENABLE(Resource)
	public:
		/**
		 * @return whether this layer is currently overriding fixture channels
		 */
		bool getIsActive() const { return mPlayer != nullptr && mPlayer->getIsPlaying(); }

		std::string				mName;				///< Property: 'Name'
		ResourcePtr<SequencePlayer>	mPlayer;		///< Property: 'Player' the sequence player driving this layer's curves
		int						mPriority = 0;		///< Property: 'Priority' higher priority layers win when multiple active layers override the same channel
	};
}
