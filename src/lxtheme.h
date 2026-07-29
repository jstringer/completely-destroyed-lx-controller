#pragma once

#include <imgui/imgui.h>

/**
 * lxtheme: semantic accent colors for the lxcontrol GUI (findings §5, "terminal / luminous").
 *
 * The dark base + widget sizing come from the HyperDark IMGuiServiceConfiguration in
 * data/config.json (wired via app.json's ServiceConfig). These are the four-hue state accents
 * the app applies per-widget on top of that base: armed/selection, live output, modulation
 * motion, plus danger/pulse/muted for destructive, awaiting-input, and secondary text.
 *
 * Accessor functions rather than constants: ImGui 1.76's ImVec4 ctor isn't constexpr.
 * ponytail: colors only. The B3 widget helpers (SectionHeader/DangerButton/DisabledRegion/
 * LabeledCombo/ModPlot) are deferred to Phase E, authored next to their first real callers
 * instead of scaffolded ahead of use.
 */
namespace lxtheme
{
	inline ImVec4 accent()	{ return { 0.20f, 0.85f, 0.82f, 1.0f }; }	// teal/cyan  - selection, armed
	inline ImVec4 live()	{ return { 1.00f, 0.78f, 0.20f, 1.0f }; }	// gold       - live output / active program
	inline ImVec4 mod()		{ return { 0.66f, 0.45f, 1.00f, 1.0f }; }	// violet     - modulation motion
	inline ImVec4 danger()	{ return { 1.00f, 0.35f, 0.35f, 1.0f }; }	// red        - destructive action
	inline ImVec4 pulse()	{ return { 1.00f, 0.60f, 0.00f, 1.0f }; }	// amber      - learn / awaiting input
	inline ImVec4 muted()	{ return { 0.55f, 0.55f, 0.58f, 1.0f }; }	// gray       - secondary text
}
