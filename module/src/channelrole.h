#pragma once

namespace lx
{
	/**
	 * Semantic role a fixture channel plays, independent of its DMX address. An Patch parameter
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
		Generic,
		/// A source that has not been given a role yet. Matches no rig channel by construction, so an unset
		/// source claims nothing -- and the editor says so rather than guessing a role the user then has to
		/// undo. Never authored on a fixture channel; only on a freshly created PatchParameter.
		Unset
	};

	/**
	 * Which set of sources a parameter of a given role spreads over. Red/Green/Blue address one colour
	 * unit each, so a group of three six-unit fixtures has 18 ColourUnit sources but only 3 Fixture
	 * sources. This is the whole reason "one effect, any number of sources" works without the user ever
	 * declaring a count.
	 */
	enum class ESpreadClass : int
	{
		Fixture,	///< one source per fixture (Dimmer, Strobe, ColorMacro, SoundMode, Generic)
		ColourUnit	///< one source per colour unit per fixture (Red, Green, Blue)
	};

	inline ESpreadClass spreadClassOf(EChannelRole role)
	{
		switch (role)
		{
		case EChannelRole::Red:
		case EChannelRole::Green:
		case EChannelRole::Blue:	return ESpreadClass::ColourUnit;
		default:					return ESpreadClass::Fixture;
		}
	}
}
