#pragma once

#include <imgui/imgui.h>
#include "lxagent.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <functional>
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
	inline ImVec4 slab()	{ return rgb(0x0e1821); }	// elevation block (borderless fill)
	inline ImVec4 slab2()	{ return rgb(0x132330); }	// alternate slab fill (stacked-neighbour contrast)
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

	/** Compact-density toggle (driven from the Live Bar). Shrinks slab gutters + spine insets for a
	 *  tighter, laptop-FOH layout. Read by gutter()/SlabEnd(). */
	inline bool&  compact() { static bool v = false; return v; }
	inline float  gutter()  { return compact() ? 2.0f : 6.0f; }

	/** Knocked-out label plate (the section-boundary marker that replaces "[ LABEL ]" + a horizontal
	 *  rule): the UPPERCASE label drawn in the ground colour on a solid `fill` block. Pure draw / no
	 *  Button, so it never reads as a clickable Chip. Advances the layout cursor. */
	inline void Plate(const char* label, const ImVec4& fill)
	{
		std::string up(label);
		for (char& ch : up) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
		const ImVec2 ts = ImGui::CalcTextSize(up.c_str());
		const float px = 9.0f, py = 3.0f;
		const ImVec2 p = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddRectFilled(p, ImVec2(p.x + ts.x + px * 2.0f, p.y + ts.y + py * 2.0f), ImGui::ColorConvertFloat4ToU32(fill));
		dl->AddText(ImVec2(p.x + px, p.y + py), ImGui::ColorConvertFloat4ToU32(bg()), up.c_str());
		ImGui::Dummy(ImVec2(ts.x + px * 2.0f, ts.y + py * 2.0f));
	}

	/** UPPERCASE section header as a neutral label-plate -- NO horizontal rule. Role-coloured sections
	 *  (Source/Modulation) call Plate(label, accent) directly for a coloured plate. */
	inline void SectionHeader(const char* label)
	{
		ImGui::Spacing();
		Plate(label, muted());		// calm neutral fill (reads soft on void, softer still on a slab)
	}

	/** A small drawn padlock at the cursor (muted), for read-only markers. Advances the layout cursor. */
	inline void Lock()
	{
		const float h = ImGui::GetTextLineHeight();
		const ImVec2 p = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImU32 col = ImGui::ColorConvertFloat4ToU32(muted());
		const float w = h * 0.60f;
		const float bx = p.x + (h - w) * 0.5f;
		const float by = p.y + h * 0.48f;			// body top
		const float pi = 3.14159265f;
		dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + w, p.y + h * 0.92f), col, 1.0f);	// body
		dl->PathArcTo(ImVec2(bx + w * 0.5f, by), w * 0.30f, pi, pi * 2.0f, 10);	// shackle (top half)
		dl->PathStroke(col, false, 1.4f);
		ImGui::Dummy(ImVec2(h * 0.75f, h));
	}

	namespace detail
	{
		inline ImVec2& slabMin()   { static ImVec2 v; return v; }
		inline float&  slabW()     { static float v = 0.0f; return v; }
		inline ImU32&  slabFill()  { static ImU32 v = 0; return v; }
		inline ImU32&  slabSpine() { static ImU32 v = 0; return v; }
		inline float&  slabPad()   { static float v = 0.0f; return v; }
	}

	/** Borderless elevation slab for a COLLECTION of related fields. Fills the block with `fill` and draws
	 *  a full-height 4px `spine` (pass a transparent colour for no spine). Use a spine only where it MEANS
	 *  something: a collection distinguished from sibling collections in whitespace, a selected list item,
	 *  or a live/active state -- not on everything. Fill + spine are drawn BEHIND the content via draw-list
	 *  channel splitting (block height isn't known until layout). Usage: SlabBegin(fill,spine); ...; SlabEnd();
	 *  ponytail: single-level -- slabs are never nested (one splitter at a time in ImGui 1.76). */
	inline void SlabBegin(const ImVec4& fill, const ImVec4& spine, float pad = 9.0f)
	{
		detail::slabMin()   = ImGui::GetCursorScreenPos();
		detail::slabW()     = ImGui::GetContentRegionAvail().x;
		detail::slabFill()  = ImGui::ColorConvertFloat4ToU32(fill);
		detail::slabSpine() = ImGui::ColorConvertFloat4ToU32(spine);
		detail::slabPad()   = pad;
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->ChannelsSplit(2);
		dl->ChannelsSetCurrent(1);				// content on the front channel
		ImGui::BeginGroup();
		ImGui::Indent(pad + 8.0f);				// clear the 4px spine + a gap
		ImGui::Dummy(ImVec2(0.0f, pad * 0.5f));
	}
	inline void SlabEnd()
	{
		const float pad = detail::slabPad();
		ImGui::Dummy(ImVec2(0.0f, pad * 0.5f));
		ImGui::Unindent(pad + 8.0f);
		ImGui::EndGroup();
		const ImVec2 mn = detail::slabMin();
		const float  bottom = ImGui::GetItemRectMax().y;
		const float  w = detail::slabW();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->ChannelsSetCurrent(0);				// fill + spine behind the content
		dl->AddRectFilled(mn, ImVec2(mn.x + w, bottom), detail::slabFill());
		dl->AddRectFilled(mn, ImVec2(mn.x + 4.0f, bottom), detail::slabSpine());		// full-height spine
		dl->ChannelsMerge();
		ImGui::Dummy(ImVec2(0.0f, gutter()));	// gutter beat to the next slab
	}

	/** A small non-interactive framed tag. `col` overrides the text color (w<=0 = default text2). */
	inline void Chip(const char* label, const ImVec4& col = ImVec4(0, 0, 0, 0))
	{
		ImGui::PushStyleColor(ImGuiCol_Button, bar());
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bar());
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, bar());
		ImGui::PushStyleColor(ImGuiCol_Text, col.w > 0.0f ? col : text2());
		ImGui::SmallButton(label);
		ImGui::PopStyleColor(4);
	}

	/** Red-tinted button for destructive actions (delete/all-stop). Returns true when clicked. */
	inline bool DangerButton(const char* label, const ImVec2& size = ImVec2(0, 0))
	{
		const ImVec4 d = danger();
		ImGui::PushStyleColor(ImGuiCol_Button,			ImVec4(d.x * 0.55f, d.y * 0.20f, d.z * 0.20f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered,	ImVec4(d.x * 0.80f, d.y * 0.30f, d.z * 0.30f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive,	d);
		const bool clicked = lxagent::Button(label, size);	// agent-clickable by label (All Stop, deletes)
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

	// --- Left-labelled inputs (ImGui draws widget labels on the RIGHT; these put the label on the LEFT) ---
	inline bool LabeledDrag(const char* label, float* v, float speed, float lo, float hi, float w = 64.0f, const char* fmt = "%.3f")
	{
		ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(label); ImGui::SameLine();
		ImGui::SetNextItemWidth(w);
		return ImGui::DragFloat((std::string("##") + label).c_str(), v, speed, lo, hi, fmt);
	}
	inline bool LabeledSlider(const char* label, float* v, float lo, float hi, float w = 90.0f)
	{
		ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(label); ImGui::SameLine();
		ImGui::SetNextItemWidth(w);
		return ImGui::SliderFloat((std::string("##") + label).c_str(), v, lo, hi);
	}
	inline bool LabeledInt(const char* label, int* v, float w = 84.0f)
	{
		ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(label); ImGui::SameLine();
		ImGui::SetNextItemWidth(w);
		return ImGui::InputInt((std::string("##") + label).c_str(), v);
	}
	inline bool LabeledCheck(const char* label, bool* v)
	{
		ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(label); ImGui::SameLine();
		return ImGui::Checkbox((std::string("##") + label).c_str(), v);
	}

	/** Violet line plot for a modulator's value history (values expected 0..1). `size.x<=0` fills the
	 *  available width (ImGui::PlotLines only auto-fills on x==0, not on -1). */
	inline void ModPlot(const char* label, const float* values, int count, const ImVec2& size = ImVec2(480, 50))
	{
		ImVec2 sz = size;
		if (sz.x <= 0.0f) sz.x = ImGui::GetContentRegionAvail().x;
		if (sz.y <= 0.0f) sz.y = 40.0f;
		ImGui::PushStyleColor(ImGuiCol_PlotLines, mod());
		ImGui::PlotLines(label, values, count, 0, nullptr, 0.0f, 1.0f, sz);
		ImGui::PopStyleColor();
	}

	/** The mockup's modulator preview: draws the STATIC shape `values` (0..1, one full cycle/envelope)
	 *  as a violet filled curve, and a playhead marker at `phase01` (the real transport position;
	 *  pass <0 to hide the marker, e.g. when the modulator isn't playing). `size.x<=0` fills the width. */
	inline void PlayheadPreview(const float* values, int count, float phase01, const ImVec2& size_arg)
	{
		ImVec2 size = size_arg;
		if (size.x <= 0.0f) size.x = ImGui::GetContentRegionAvail().x;	// -1 => fill width (not raw -1px)
		if (size.y <= 0.0f) size.y = 50.0f;
		const ImVec2 p = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), ImGui::ColorConvertFloat4ToU32(well()));
		dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), ImGui::ColorConvertFloat4ToU32(border()));
		if (count > 1)
		{
			const ImU32 line = ImGui::ColorConvertFloat4ToU32(mod());
			const ImU32 fill = ImGui::ColorConvertFloat4ToU32(ImVec4(mod().x, mod().y, mod().z, 0.14f));
			auto yat = [&](int i) { float v = values[i]; v = v < 0 ? 0 : (v > 1 ? 1 : v); return p.y + size.y * (1.0f - v); };
			for (int i = 0; i < count - 1; ++i)
			{
				const float x0 = p.x + size.x * i / (count - 1), x1 = p.x + size.x * (i + 1) / (count - 1);
				dl->AddQuadFilled(ImVec2(x0, yat(i)), ImVec2(x1, yat(i + 1)), ImVec2(x1, p.y + size.y), ImVec2(x0, p.y + size.y), fill);
				dl->AddLine(ImVec2(x0, yat(i)), ImVec2(x1, yat(i + 1)), line, 2.0f);
			}
		}
		if (phase01 >= 0.0f)
		{
			const float ph = phase01 > 1.0f ? 1.0f : phase01;
			const float px = p.x + size.x * ph;
			dl->AddLine(ImVec2(px, p.y), ImVec2(px, p.y + size.y), ImGui::ColorConvertFloat4ToU32(mod2()), 1.5f);
		}
		ImGui::Dummy(size);
	}

	/**
	 * Horizontal field strip: a modulator (or a patch parameter's resolved field) sampled as a continuous
	 * function of position 0..1 -- the readout that replaces the per-voice meter row. `components` >= 3
	 * draws RGB; otherwise the gold luminance ramp (a float field must not invent a fourth hue).
	 *
	 * One sample per PIXEL COLUMN, deliberately. The fields that matter most are hard-edged (Chase's pulse,
	 * Noise at Smoothing 0) and coarser sampling plus interpolation smears those edges or misses them
	 * outright -- a Chase at PulseWidth 0.05 can vanish between samples. Per-column is also the simplest
	 * thing that works: no segment count to tune, no interpolation mode to choose.
	 * ponytail: if this ever profiles hot the upgrade is PrimReserve + direct vertex writes, NOT a coarser
	 * sample count -- coarser sampling buys frames by drawing something the rig isn't doing.
	 */
	inline void FieldStrip(const std::function<float(float pos01, int component)>& field,
	                       int components, const ImVec2& size_arg)
	{
		ImVec2 size = size_arg;
		if (size.x <= 0.0f) size.x = ImGui::GetContentRegionAvail().x;	// -1 => fill width, not raw -1px
		if (size.y <= 0.0f) size.y = 28.0f;

		const ImVec2 p = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const int cols = std::max(2, static_cast<int>(size.x));
		auto sat = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };

		for (int i = 0; i < cols; ++i)
		{
			const float pos = static_cast<float>(i) / (cols - 1);
			ImU32 col;
			if (components >= 3)
			{
				col = ImGui::ColorConvertFloat4ToU32(
					ImVec4(sat(field(pos, 0)), sat(field(pos, 1)), sat(field(pos, 2)), 1.0f));
			}
			else
			{
				const float v = sat(field(pos, 0));
				const ImVec4 g = live();
				col = ImGui::ColorConvertFloat4ToU32(ImVec4(g.x * v, g.y * v, g.z * v, 1.0f));
			}
			const float x0 = p.x + size.x * i / cols;
			const float x1 = p.x + size.x * (i + 1) / cols;
			dl->AddRectFilled(ImVec2(x0, p.y), ImVec2(x1 + 0.5f, p.y + size.y), col);	// .5: no sub-pixel seams
		}
		// Frame it, like PlayheadPreview: a field that is legitimately dark everywhere (a Replace-blend
		// modulator that isn't playing) would otherwise be indistinguishable from empty space.
		dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), ImGui::ColorConvertFloat4ToU32(border()));
		ImGui::Dummy(size);
	}

	/**
	 * One modulatable Field input: uppercase label, its value, then EITHER a slider (authored) or a violet
	 * driven-by marker when a Curve modulator owns it.
	 *
	 * Hiding the slider is correctness, not decoration: pass 1 of Patch::update overwrites a driven input
	 * every frame, so an editable slider there would visibly fight the modulator.
	 * @return true when the slider changed.
	 */
	inline bool InputRow(const char* label, float* v01, const char* drivenBy)
	{
		ImGui::AlignTextToFramePadding();
		ImGui::PushStyleColor(ImGuiCol_Text, drivenBy != nullptr ? mod2() : muted());
		ImGui::TextUnformatted(label);
		ImGui::PopStyleColor();
		ImGui::SameLine(122.0f);	// clears the longest input label ("Smoothing") in the mono face
		ImGui::AlignTextToFramePadding();
		ImGui::Text("%.2f", *v01);
		ImGui::SameLine(176.0f);

		if (drivenBy != nullptr)
		{
			// 4px violet stub + the driver's name: the spine grammar, at row scale.
			const ImVec2 p = ImGui::GetCursorScreenPos();
			const float h = ImGui::GetTextLineHeight();
			ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + 4.0f, p.y + h),
				ImGui::ColorConvertFloat4ToU32(mod()));
			ImGui::Dummy(ImVec2(10.0f, h));
			ImGui::SameLine();
			ImGui::TextColored(mod2(), "driven by %s", drivenBy);
			return false;
		}

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::PushStyleColor(ImGuiCol_SliderGrab, mod());
		const std::string id = std::string("##in") + label;
		const bool changed = ImGui::SliderFloat(id.c_str(), v01, 0.0f, 1.0f, "");
		ImGui::PopStyleColor();
		return changed;
	}

	/**
	 * "This cannot affect output yet" -- an unset source role, or a modulator with no target. Amber, matching
	 * the `!unbound` marker the routing rows already use.
	 *
	 * Deliberately does NOT touch the spine: a gold spine already means "this fixture is emitting", so an
	 * inert card wearing it would read exactly backwards.
	 */
	inline void WarnChip(const char* label)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, rgb(0xf5b301, 0.14f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgb(0xf5b301, 0.14f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgb(0xf5b301, 0.14f));
		ImGui::PushStyleColor(ImGuiCol_Text, live2());
		ImGui::SmallButton(label);
		ImGui::PopStyleColor(4);
	}

	/** Pushes amber text for an unset picker so it reads as a prompt rather than a value. Pair with
	 *  PopPrompt. Returns `unset` so the caller can pass it straight back. */
	inline bool PushPrompt(bool unset)
	{
		if (unset)
			ImGui::PushStyleColor(ImGuiCol_Text, live2());
		return unset;
	}
	inline void PopPrompt(bool unset) { if (unset) ImGui::PopStyleColor(); }

	/** Fold toggle: `-` when open (click to fold), `+` when folded. ASCII on purpose -- lxagent::fold drops
	 *  non-ASCII, so a glyph chevron would be unaddressable from the agent bridge. Flips `*collapsed` and
	 *  returns true when it changed, so the caller can mark content dirty.
	 *
	 *  `id` is REQUIRED and must be unique within the enclosing ImGui ID scope: the label is the widget's
	 *  ID, so three toggles all labelled "-" at the same level are the SAME widget and only the first drawn
	 *  responds. That is exactly how the CURRENT / SOURCE / MODULATION folds silently stopped working.
	 *  The "##" suffix is invisible and lxagent::fold strips it, so the agent key stays "-" / "+". */
	inline bool FoldToggle(bool* collapsed, const char* id)
	{
		const std::string label = std::string(*collapsed ? "+" : "-") + "##fold" + id;
		if (lxagent::SmallButton(label.c_str()))
		{
			*collapsed = !*collapsed;
			return true;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(*collapsed ? "Show editable params" : "Fold away the params, keep the readout");
		return false;
	}

	/** InputRow's colour sibling (a Gradient endpoint): swatch instead of slider, same driven-by treatment.
	 *  @return true when the swatch changed. */
	inline bool ColorInputRow(const char* label, float* r, float* g, float* b, const char* drivenBy)
	{
		ImGui::AlignTextToFramePadding();
		ImGui::PushStyleColor(ImGuiCol_Text, drivenBy != nullptr ? mod2() : muted());
		ImGui::TextUnformatted(label);
		ImGui::PopStyleColor();
		ImGui::SameLine(122.0f);

		float col[3] = { *r, *g, *b };
		if (drivenBy != nullptr)
		{
			// Read-only swatch + the driver's name: pass 1 rewrites this every frame. (Drawn inline rather
			// than via Swatch(), which is declared further down this header.)
			ImGui::ColorButton(label, ImVec4(col[0], col[1], col[2], 1.0f),
				ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoTooltip,
				ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
			ImGui::SameLine();
			const ImVec2 p = ImGui::GetCursorScreenPos();
			const float h = ImGui::GetTextLineHeight();
			ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + 4.0f, p.y + h),
				ImGui::ColorConvertFloat4ToU32(mod()));
			ImGui::Dummy(ImVec2(10.0f, h));
			ImGui::SameLine();
			ImGui::TextColored(mod2(), "driven by %s", drivenBy);
			return false;
		}

		const std::string id = std::string("##col") + label;
		if (ImGui::ColorEdit3(id.c_str(), col, ImGuiColorEditFlags_NoInputs))
		{
			*r = col[0]; *g = col[1]; *b = col[2];
			return true;
		}
		return false;
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
