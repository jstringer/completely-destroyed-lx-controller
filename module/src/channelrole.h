#pragma once

namespace lx
{
	/**
	 * Semantic role a fixture channel plays, independent of its DMX address. An Effect parameter
	 * targets a role (+ optional unit index); mapping it to a fixture auto-binds it to the matching
	 * channel(s). RGB is expressed as Red/Green/Blue + a per-channel UnitIndex (1..6 for the
	 * Eurolite's six SMD units); PresetColor -> ColorMacro, AutoSound -> SoundMode.
	 */
	enum class EChannelRole : int
	{
		Dimmer,
		Strobe,
		Red,
		Green,
		Blue,
		ColorMacro,
		SoundMode,
		Generic
	};

	/**
	 * DMX value width of a channel. Only Value8 is emitted today; the 16-bit variants are reserved
	 * for future pan/tilt-style channels.
	 */
	enum class EDmxChannelWidth : int
	{
		Value8,
		Value16MSB,
		Value16LSB
	};
}
