#pragma once

// External Includes
#include <nap/resource.h>
#include <nap/resourceptr.h>

// Local Includes
#include "fixturechannel.h"

namespace nap
{
	/**
	 * A named fixture template, e.g. "RGB Par" or "Moving Head", describing its channel layout.
	 */
	class NAPAPI FixtureType : public Resource
	{
		RTTI_ENABLE(Resource)
	public:
		/**
		 * Validates that channel names and offsets are unique.
		 */
		virtual bool init(utility::ErrorState& errorState) override;

		/**
		 * @param name the channel name to look for, e.g. "Dimmer"
		 * @return the channel with the given name, nullptr if not found
		 */
		const FixtureChannel* findChannel(const std::string& name) const;

		std::vector<ResourcePtr<FixtureChannel>> mChannels;	///< Property: 'Channels'
	};
}
