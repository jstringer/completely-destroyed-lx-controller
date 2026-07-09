#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <artnetcontroller.h>

// Local Includes
#include "fixturetype.h"
#include "fixturechannelbinding.h"

namespace nap
{
	/**
	 * A physical, patched fixture instance: a FixtureType, the ArtNetController/universe
	 * it's patched into, its starting channel, and the per-channel value bindings that
	 * drive it. init() cross-checks that every FixtureType channel has a matching binding
	 * and precomputes each channel's absolute DMX channel number.
	 */
	class NAPAPI Fixture : public Resource
	{
		RTTI_ENABLE(Resource)
	public:
		struct ResolvedChannel
		{
			int							mAbsoluteChannel = 0;
			FixtureChannelBinding*		mBinding = nullptr;
		};

		virtual bool init(utility::ErrorState& errorState) override;

		ResourcePtr<FixtureType>						mFixtureType;		///< Property: 'FixtureType'
		ResourcePtr<ArtNetController>					mController;		///< Property: 'Controller' the universe this fixture is patched into
		int												mStartChannel = 0;	///< Property: 'StartChannel' zero-based offset within the controller's channels
		std::vector<ResourcePtr<FixtureChannelBinding>>	mChannelBindings;	///< Property: 'ChannelBindings'

		std::vector<ResolvedChannel> mResolvedChannels;	///< Cached channel/binding pairs, built in init()
	};
}
