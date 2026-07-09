#pragma once

// External Includes
#include <nap/resource.h>

namespace nap
{
	/**
	 * The kind of DMX value a FixtureChannel represents.
	 * Only Value8 is used for now; the 16-bit variants are reserved for future pan/tilt support.
	 */
	enum class EDmxChannelType : int
	{
		Value8		= 0,	///< Single 8-bit DMX channel
		Value16MSB	= 1,	///< Most-significant byte of a 16-bit value (reserved)
		Value16LSB	= 2		///< Least-significant byte of a 16-bit value (reserved)
	};

	/**
	 * Describes one named channel slot within a FixtureType, e.g. "Dimmer" at offset 0.
	 */
	class NAPAPI FixtureChannel : public Resource
	{
		RTTI_ENABLE(Resource)
	public:
		std::string			mName;								///< Property: 'Name' e.g. "Dimmer", "R", "Pan"
		int					mOffset = 0;						///< Property: 'Offset' zero-based offset within the fixture's channel block
		EDmxChannelType		mType = EDmxChannelType::Value8;	///< Property: 'ChannelType' ("Type" is reserved by the RTTI JSON schema)
	};
}
