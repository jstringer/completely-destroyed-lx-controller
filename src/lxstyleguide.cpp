#include "lxstyleguide.h"
#include "lxtheme.h"

#include <imgui/imgui.h>
#include <cmath>
#include <vector>

namespace lxstyleguide
{
	// One labeled color chip + its name, two per row.
	static void paletteRow(const char* name, const ImVec4& c, bool secondInRow)
	{
		ImGui::ColorButton(name, c, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoTooltip, ImVec2(26, 26));
		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(name);
		if (!secondInRow)
			ImGui::SameLine(ImGui::GetWindowWidth() * 0.5f);
	}


	void draw(bool* open)
	{
		if (open != nullptr && !*open)
			return;

		ImGui::SetNextWindowSize(ImVec2(580, 660), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Design Language", open))
		{
			ImGui::End();
			return;
		}

		const float t = static_cast<float>(ImGui::GetTime());

		ImGui::TextDisabled("lxcontrol design language - live sandbox (fake data).");
		ImGui::TextDisabled("Edit src/lxstyleguide.cpp / src/lxtheme.h; Phase E consumes these helpers.");

		// --- Palette -------------------------------------------------------
		lxtheme::SectionHeader("Palette");
		paletteRow("accent", lxtheme::accent(), false);	paletteRow("live",   lxtheme::live(),   true);
		paletteRow("mod",    lxtheme::mod(),    false);	paletteRow("danger", lxtheme::danger(), true);
		paletteRow("pulse",  lxtheme::pulse(),  false);	paletteRow("muted",  lxtheme::muted(),  true);

		// --- Typography ----------------------------------------------------
		lxtheme::SectionHeader("Typography");
		ImGui::TextUnformatted("Body text - the quick brown fox jumps over the lazy dog");
		ImGui::TextColored(lxtheme::accent(), "Accent text (armed / selection)");
		ImGui::TextColored(lxtheme::live(),   "Live text (output / active program)");
		ImGui::TextColored(lxtheme::mod(),    "Modulation text (motion)");
		ImGui::TextDisabled("Disabled / muted secondary text");

		// --- Buttons -------------------------------------------------------
		lxtheme::SectionHeader("Buttons");
		ImGui::Button("Normal");
		ImGui::SameLine();
		lxtheme::DangerButton("Delete");
		ImGui::SameLine();
		const bool dis = lxtheme::PushDisabled(true);
		ImGui::Button("Disabled");
		lxtheme::PopDisabled(dis);

		// --- State system --------------------------------------------------
		lxtheme::SectionHeader("State system (four-hue)");
		lxtheme::StateDot(ImVec4(0.25f, 0.25f, 0.28f, 1.0f));	ImGui::SameLine(); ImGui::Text("dark - no output");
		lxtheme::StateDot(lxtheme::accent(), true);				ImGui::SameLine(); ImGui::Text("armed - breathing cyan");
		lxtheme::StateDot(lxtheme::live());						ImGui::SameLine(); ImGui::Text("live - gold");
		lxtheme::StateDot(lxtheme::mod(), true);				ImGui::SameLine(); ImGui::Text("modulating - breathing violet");

		// --- Inputs --------------------------------------------------------
		lxtheme::SectionHeader("Inputs");
		static int mode = 0;
		const char* modes[] = { "Hold", "Latch", "Trig" };
		lxtheme::LabeledCombo("Control mode", &mode, modes, 3);
		static float col[3] = { 0.90f, 0.30f, 0.20f };
		ImGui::ColorEdit3("Editable swatch", col, ImGuiColorEditFlags_NoInputs);

		// --- Modulation plot ----------------------------------------------
		lxtheme::SectionHeader("Modulation plot");
		static std::vector<float> wave(128);
		for (int i = 0; i < 128; ++i)
			wave[i] = 0.5f + 0.5f * std::sin(t * 2.0f + i * 0.15f);
		lxtheme::ModPlot("##modplot", wave.data(), 128, ImVec2(520, 50));

		// --- Fixture strip preview (RIG surface) --------------------------
		lxtheme::SectionHeader("Fixture strip (RIG preview)");
		for (int f = 0; f < 3; ++f)
		{
			ImGui::PushID(f);
			ImGui::BeginGroup();
			ImGui::Text("Strobe %d", f + 1);

			const float dim    = 0.5f + 0.5f * std::sin(t + f);
			const float strobe = 0.5f + 0.5f * std::sin(t * 1.7f + f * 1.3f);
			lxtheme::Fader("##dim", dim, ImVec2(18, 84), lxtheme::live());
			ImGui::SameLine();
			lxtheme::Fader("##strobe", strobe, ImVec2(18, 84), lxtheme::accent());

			float rgb[3] = {
				0.5f + 0.5f * std::sin(t + f),
				0.5f + 0.5f * std::sin(t * 1.3f + f),
				0.5f + 0.5f * std::sin(t * 0.7f + f)
			};
			lxtheme::Swatch("##sw", rgb, 40.0f);

			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("out");
			ImGui::SameLine();
			lxtheme::OutputLed(dim > 0.5f, lxtheme::live());
			ImGui::SameLine();
			lxtheme::OutputLed(strobe > 0.6f, lxtheme::accent());
			ImGui::EndGroup();
			ImGui::PopID();

			if (f < 2)
				ImGui::SameLine(0.0f, 24.0f);
		}

		ImGui::End();
	}
}
