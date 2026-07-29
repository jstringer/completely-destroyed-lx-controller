#pragma once

#include <imgui/imgui.h>
#include <cmath>
#include <string>

/**
 * lxtheme: the lxcontrol design language (findings §5, "terminal / luminous").
 *
 * Two parts:
 *  - semantic accent colors (accent/live/mod/danger/pulse/muted) on top of the HyperDark base
 *    set by the IMGuiServiceConfiguration in data/config.json (wired via app.json ServiceConfig);
 *  - reusable widget helpers, all thin wrappers over vendored ImGui 1.76 (no imgui_internal deps).
 *
 * Every helper here is exercised live in the Style Guide test bed (src/lxstyleguide.cpp) and
 * consumed by the real surfaces in Phase E. Accessor functions rather than constants because
 * ImGui 1.76's ImVec4 ctor isn't constexpr.
 */
namespace lxtheme
{
	// --- Palette (mockup v5 :root, exact hex) -------------------------------
	inline ImVec4 rgb(int hex, float a = 1.0f)
	{
		return { ((hex >> 16) & 0xff) / 255.0f, ((hex >> 8) & 0xff) / 255.0f, (hex & 0xff) / 255.0f, a };
	}
	// Surfaces
	inline ImVec4 bg()		{ return rgb(0x06090c); }	// window background
	inline ImVec4 well()	{ return rgb(0x03060a); }	// recessed panel / input well
	inline ImVec4 bar()		{ return rgb(0x0a1015); }	// title / header bars
	inline ImVec4 border()	{ return rgb(0x183640); }
	inline ImVec4 borderHi(){ return rgb(0x22505c); }
	inline ImVec4 btnbg()	{ return rgb(0x0c141a); }	// button background
	// Text
	inline ImVec4 text()	{ return rgb(0xe3f4f7); }
	inline ImVec4 text2()	{ return rgb(0x9dbdc5); }
	inline ImVec4 muted()	{ return rgb(0x5f818c); }	// secondary text
	// Semantic accents
	inline ImVec4 accent()	{ return rgb(0x2dd4bf); }	// teal   - you / armed / selection
	inline ImVec4 accent2()	{ return rgb(0x7cf5ff); }	// cyan   - hover / active-you
	inline ImVec4 live()	{ return rgb(0xf5b301); }	// gold   - light is LIVE
	inline ImVec4 live2()	{ return rgb(0xffce4d); }
	inline ImVec4 mod()		{ return rgb(0xa78bfa); }	// violet - modulation
	inline ImVec4 mod2()	{ return rgb(0xc4b5fd); }
	inline ImVec4 danger()	{ return rgb(0xfb7185); }	// rose   - destructive
	inline ImVec4 pulse()	{ return rgb(0x7cf5ff); }	// cyan   - listening / awaiting input (breathing)

	/** Writes the full terminal/luminous style into ImGui::GetStyle(). Call once per frame BEFORE any
	 *  window is submitted (top of drawMainUI) so it wins over whatever the SDK scheme set. */
	inline void applyStyle()
	{
		ImGuiStyle& s = ImGui::GetStyle();
		// Geometry: sharp, terminal-like.
		s.WindowRounding = 0.0f; s.ChildRounding = 0.0f; s.FrameRounding = 0.0f;
		s.PopupRounding = 0.0f; s.ScrollbarRounding = 0.0f; s.GrabRounding = 0.0f; s.TabRounding = 0.0f;
		s.WindowBorderSize = 1.0f; s.ChildBorderSize = 1.0f; s.FrameBorderSize = 1.0f; s.PopupBorderSize = 1.0f;
		s.WindowPadding = ImVec2(14, 12); s.FramePadding = ImVec2(9, 5); s.ItemSpacing = ImVec2(10, 6);
		s.ItemInnerSpacing = ImVec2(8, 6); s.IndentSpacing = 18.0f; s.ScrollbarSize = 12.0f; s.GrabMinSize = 8.0f;

		ImVec4* c = s.Colors;
		c[ImGuiCol_Text]				= text();
		c[ImGuiCol_TextDisabled]		= muted();
		c[ImGuiCol_WindowBg]			= bg();
		c[ImGuiCol_ChildBg]				= well();
		c[ImGuiCol_PopupBg]				= bar();
		c[ImGuiCol_Border]				= border();
		c[ImGuiCol_BorderShadow]		= ImVec4(0, 0, 0, 0);
		c[ImGuiCol_FrameBg]				= well();
		c[ImGuiCol_FrameBgHovered]		= bar();
		c[ImGuiCol_FrameBgActive]		= rgb(0x183640, 0.6f);
		c[ImGuiCol_TitleBg]				= bar();
		c[ImGuiCol_TitleBgActive]		= bar();
		c[ImGuiCol_TitleBgCollapsed]	= bar();
		c[ImGuiCol_MenuBarBg]			= bar();
		c[ImGuiCol_ScrollbarBg]			= well();
		c[ImGuiCol_ScrollbarGrab]		= border();
		c[ImGuiCol_ScrollbarGrabHovered]= borderHi();
		c[ImGuiCol_ScrollbarGrabActive]	= accent();
		c[ImGuiCol_CheckMark]			= accent();
		c[ImGuiCol_SliderGrab]			= accent();
		c[ImGuiCol_SliderGrabActive]	= accent2();
		c[ImGuiCol_Button]				= btnbg();
		c[ImGuiCol_ButtonHovered]		= bar();
		c[ImGuiCol_ButtonActive]		= rgb(0x22505c, 0.5f);
		c[ImGuiCol_Header]				= rgb(0x2dd4bf, 0.12f);
		c[ImGuiCol_HeaderHovered]		= rgb(0x2dd4bf, 0.20f);
		c[ImGuiCol_HeaderActive]		= rgb(0x2dd4bf, 0.30f);
		c[ImGuiCol_Separator]			= border();
		c[ImGuiCol_SeparatorHovered]	= borderHi();
		c[ImGuiCol_SeparatorActive]		= accent();
		c[ImGuiCol_Tab]					= bar();
		c[ImGuiCol_TabHovered]			= rgb(0x2dd4bf, 0.20f);
		c[ImGuiCol_TabActive]			= rgb(0x2dd4bf, 0.16f);
		c[ImGuiCol_TabUnfocused]		= bar();
		c[ImGuiCol_TabUnfocusedActive]	= well();
		c[ImGuiCol_PlotLines]			= mod();
		c[ImGuiCol_PlotLinesHovered]	= mod2();
		c[ImGuiCol_PlotHistogram]		= live();
		c[ImGuiCol_TextSelectedBg]		= rgb(0x2dd4bf, 0.30f);
		c[ImGuiCol_ResizeGrip]			= border();
		c[ImGuiCol_ResizeGripHovered]	= borderHi();
		c[ImGuiCol_ResizeGripActive]	= accent();
	}

	// --- Widget helpers -----------------------------------------------------

	/** Accent-colored label + separator; opens a titled block. */
	inline void SectionHeader(const char* label)
	{
		ImGui::Spacing();
		ImGui::TextColored(accent(), "%s", label);
		ImGui::Separator();
	}

	/** Red-tinted button for destructive actions (delete/all-stop). Returns true when clicked. */
	inline bool DangerButton(const char* label, const ImVec2& size = ImVec2(0, 0))
	{
		const ImVec4 d = danger();
		ImGui::PushStyleColor(ImGuiCol_Button,			ImVec4(d.x * 0.55f, d.y * 0.20f, d.z * 0.20f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered,	ImVec4(d.x * 0.80f, d.y * 0.30f, d.z * 0.30f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive,	d);
		const bool clicked = ImGui::Button(label, size);
		ImGui::PopStyleColor(3);
		return clicked;
	}

	/** Gray-out for a disabled region (ImGui 1.76 predates BeginDisabled/EndDisabled). Pass the same
	 *  bool to PopDisabled. NOTE: this only dims -- the caller must still gate the click on !disabled,
	 *  same idiom the app already uses for "+ Add Control binding". Returns `disabled` for convenience. */
	inline bool PushDisabled(bool disabled)
	{
		if (disabled)
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		return disabled;
	}
	inline void PopDisabled(bool disabled)
	{
		if (disabled)
			ImGui::PopStyleVar();
	}

	/** A combo with a left-aligned label (the ## id keeps the visible label on the left, not ImGui's right). */
	inline bool LabeledCombo(const char* label, int* current, const char* const items[], int count, float itemWidth = 160.0f)
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(itemWidth);
		const std::string id = std::string("##") + label;
		return ImGui::Combo(id.c_str(), current, items, count);
	}

	/** Violet line plot for a modulator's value history (values expected 0..1). */
	inline void ModPlot(const char* label, const float* values, int count, const ImVec2& size = ImVec2(480, 50))
	{
		ImGui::PushStyleColor(ImGuiCol_PlotLines, mod());
		ImGui::PlotLines(label, values, count, 0, nullptr, 0.0f, 1.0f, size);
		ImGui::PopStyleColor();
	}

	/** Filled state dot at the cursor. `pulsing` breathes alpha+radius off ImGui::GetTime() (armed/modulating). */
	inline void StateDot(const ImVec4& color, bool pulsing = false, float radius = 5.0f)
	{
		const float line = ImGui::GetTextLineHeight();
		const ImVec2 p = ImGui::GetCursorScreenPos();
		float breathe = 1.0f;
		if (pulsing)
			breathe = 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 3.0f);
		ImVec4 c = color;
		c.w *= (0.45f + 0.55f * breathe);
		const float r = radius * (pulsing ? (0.82f + 0.18f * breathe) : 1.0f);
		const ImVec2 center(p.x + radius, p.y + line * 0.5f);
		ImGui::GetWindowDrawList()->AddCircleFilled(center, r, ImGui::ColorConvertFloat4ToU32(c), 16);
		ImGui::Dummy(ImVec2(radius * 2.0f, line));
	}

	/** Bottom-up filling bar (display-only fader for the RIG strips). `value01` clamped 0..1. */
	inline void Fader(const char* id, float value01, const ImVec2& size, const ImVec4& fill)
	{
		(void)id;
		value01 = value01 < 0.0f ? 0.0f : (value01 > 1.0f ? 1.0f : value01);
		const ImVec2 p = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImU32 bg = ImGui::ColorConvertFloat4ToU32(ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
		const ImU32 fg = ImGui::ColorConvertFloat4ToU32(fill);
		dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), bg, 2.0f);
		const float h = size.y * value01;
		dl->AddRectFilled(ImVec2(p.x, p.y + size.y - h), ImVec2(p.x + size.x, p.y + size.y), fg, 2.0f);
		ImGui::Dummy(size);
	}

	/** RGB color chip. */
	inline void Swatch(const char* id, const float rgb[3], float size = 0.0f)
	{
		const ImVec2 sz = size > 0.0f ? ImVec2(size, size) : ImVec2(0, 0);
		ImGui::ColorButton(id, ImVec4(rgb[0], rgb[1], rgb[2], 1.0f),
			ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoTooltip, sz);
	}

	/** Small per-channel output indicator: bright `onColor` when live, dim otherwise. */
	inline void OutputLed(bool on, const ImVec4& onColor)
	{
		StateDot(on ? onColor : ImVec4(0.20f, 0.20f, 0.22f, 1.0f), false, 4.0f);
	}
}
