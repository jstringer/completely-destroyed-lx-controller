#pragma once

#include <imgui/imgui.h>
#include <utility/fileutils.h>
#include <nap/logger.h>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <cctype>
#include <cstdio>

/**
 * lxagent: a drive-and-inspect bridge for the ImGui surface, so a coding agent (or any script) can
 * navigate the UI and read back what is on screen without a mouse -- Playwright-ish, for NAP.
 *
 * Why this and NOT nap::Snapshot: Snapshot renders RenderableComponents through a PerspCamera into a
 * headless target and deliberately *excludes* GUI (that's its selling point, see the nap_snapshot
 * article). lxcontrol's surface is entirely ImGui drawn into the window's render pass, so Snapshot
 * would capture the gnomon and nothing else. Pixels are therefore captured OUTSIDE the app, by
 * Win32 window capture (.claude/skills/lx-drive/scripts/lxui.ps1); this header only provides
 * *control* plus a text read-back of the frame that just drew.
 *
 * Protocol -- one command per frame, two files in `.agent/` relative to the working directory:
 *   cmd   written by the driver: "<verb> <arg>"; deleted by the app the moment it is read
 *   ack   written by the app after that frame drew: HIT/MISS + a state dump + every clickable label
 * Files rather than a socket / naposc / napapi: no new NAP module, no port, no regenerate.
 * `.agent/` sits outside data/ on purpose -- nap::ResourceManager's directory watch hot-reloads
 * what it recognises in there, and a command file landing mid-frame must not trigger that.
 *
 * Addressing is by label, not pixel: lxagent::Button("+ New Patch") is clickable as `click + New
 * Patch`. Duplicate labels within a frame (the many PushID'd "Fire"/"Stop"/"Del" rows) get an
 * occurrence suffix -- `click Fire#2` is the second "Fire" drawn this frame; the ack's CLICKABLE
 * line always lists the exact keys, so addressing never has to be guessed.
 *
 * ponytail: no synthetic pointer/key events. NAP begins the ImGui frame *before* App::update(), so
 * injected mouse state lands a frame late and a real click needs a 3-frame state machine; add that
 * (via IMGuiService::processInputEvent) only if some widget proves unreachable by label. Likewise
 * only buttons are addressable -- checkboxes/sliders/combos are read-only to the bridge for now.
 */
namespace lxagent
{
	struct Bridge
	{
		/** Command/ack directory. NAP sets the process CWD to the project *data* directory (that's why
		 *  lxcontrolService can open a bare "user_content.json"), so "../.agent" lands in the app root --
		 *  outside data/, away from nap::ResourceManager's directory watch. */
		std::string							mDir = "../.agent";
		bool								mActive = false;	///< a command is being served this frame
		bool								mHit = false;		///< that command matched something
		std::string							mCommand;			///< raw command line, echoed into the ack
		std::string							mClick;				///< label (or label#n) to click this frame
		std::string							mTab;				///< tab to select this frame
		std::vector<std::string>			mSeen;				///< clickable keys drawn this frame
		std::map<std::string, int>			mCounts;			///< per-label occurrence counter
	};

	inline Bridge& bridge() { static Bridge b; return b; }

	inline std::string path(const char* file) { return bridge().mDir + "/" + file; }

	/**
	 * The addressable key for a label: visible part only ("Del##trig" -> "Del"), every run of
	 * whitespace collapsed to one space, and non-ASCII bytes dropped. The whitespace folding is what
	 * makes the two-line Perform pad labels addressable at all (an embedded newline would also break
	 * the one-line ack format); dropping non-ASCII keeps keys typable, since a glyph would not survive
	 * the shell -> PowerShell -> file round-trip.
	 */
	inline std::string fold(const std::string& label)
	{
		const std::string vis = label.substr(0, label.find("##"));
		std::string out;
		bool pending_space = false;
		for (const unsigned char c : vis)
		{
			if (c >= 0x80)
				continue;
			if (std::isspace(c) != 0)
			{
				pending_space = !out.empty();
				continue;
			}
			if (pending_space)
			{
				out += ' ';
				pending_space = false;
			}
			out += static_cast<char>(c);
		}
		return out;
	}

	/** Trims ASCII whitespace and any wrapping double quotes. */
	inline std::string trim(const std::string& in)
	{
		size_t a = in.find_first_not_of(" \t\r\n");
		if (a == std::string::npos)
			return {};
		size_t b = in.find_last_not_of(" \t\r\n");
		std::string s = in.substr(a, b - a + 1);
		if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
			s = s.substr(1, s.size() - 2);
		return s;
	}

	/** Marks the current command as handled, for verbs the app itself acts on (mode/text/resize/...). */
	inline void hit() { bridge().mHit = true; }

	/**
	 * Reads .agent/cmd, if present, at the top of App::update(). `click` and `tab` are consumed here;
	 * every other verb is returned as {verb, arg} for the app to act on ({} when there is no command).
	 */
	inline std::pair<std::string, std::string> poll()
	{
		Bridge& b = bridge();
		b.mActive = false; b.mHit = false; b.mClick.clear(); b.mTab.clear(); b.mCommand.clear();
		b.mSeen.clear(); b.mCounts.clear();

		// Announce the resolved location once, so a driver watching the log can see where to write.
		static bool announced = false;
		if (!announced)
		{
			announced = true;
			nap::utility::ensureDirExists(b.mDir);
			nap::Logger::info("lxagent bridge: %s", nap::utility::getAbsolutePath(b.mDir).c_str());
		}

		std::ifstream in(path("cmd"));
		if (!in.good())
			return {};

		std::string line;
		std::getline(in, line);
		in.close();
		std::remove(path("cmd").c_str());

		line = trim(line);
		if (line.empty())
			return {};

		b.mActive = true;
		b.mCommand = line;

		const size_t sp = line.find(' ');
		const std::string verb = line.substr(0, sp);
		const std::string arg = sp == std::string::npos ? std::string() : trim(line.substr(sp + 1));

		if (verb == "click") { b.mClick = arg; return {}; }
		if (verb == "tab") { b.mTab = arg; return {}; }
		return { verb, arg };
	}

	/**
	 * Writes .agent/ack after the frame has been queued (end of App::update()). `status` is the app's
	 * own newline-terminated state lines; everything else the bridge knows is appended here.
	 */
	inline void finish(const std::string& status)
	{
		Bridge& b = bridge();
		if (!b.mActive)
			return;

		nap::utility::ensureDirExists(b.mDir);

		// Write then rename, so the driver never reads a half-written ack (it polls for existence).
		const std::string tmp = path("ack.tmp");
		{
			std::ofstream out(tmp, std::ios::trunc);
			out << "CMD: " << b.mCommand << "\n";
			out << "RESULT: " << (b.mHit ? "HIT" : "MISS") << "\n";
			out << status;
			out << "CLICKABLE:";
			for (const auto& s : b.mSeen)
				out << " [" << s << "]";
			out << "\n";
		}
		std::remove(path("ack").c_str());	// std::rename won't clobber on Windows
		std::rename(tmp.c_str(), path("ack").c_str());
		b.mActive = false;
	}

	/** Registers a clickable label for this frame and reports whether the driver addressed it. */
	inline bool consumeClick(const char* label)
	{
		Bridge& b = bridge();
		const std::string vis = fold(label);
		const int n = ++b.mCounts[vis];
		const std::string key = n == 1 ? vis : vis + "#" + std::to_string(n);
		b.mSeen.push_back(key);

		if (b.mClick.empty() || (b.mClick != key && b.mClick != label))
			return false;

		b.mClick.clear();
		b.mHit = true;
		return true;
	}

	/** Drop-in for ImGui::Button that is also clickable by label from the bridge. */
	inline bool Button(const char* label, const ImVec2& size = ImVec2(0, 0))
	{
		const bool clicked = ImGui::Button(label, size);
		return consumeClick(label) || clicked;
	}

	/** Drop-in for ImGui::SmallButton, same deal. */
	inline bool SmallButton(const char* label)
	{
		const bool clicked = ImGui::SmallButton(label);
		return consumeClick(label) || clicked;
	}

	/** Flags for BeginTabItem: selects the tab when the driver asked for it (ImGui 1.76 SetSelected). */
	inline ImGuiTabItemFlags tabFlags(const char* label)
	{
		Bridge& b = bridge();
		b.mSeen.push_back(std::string("tab:") + label);
		if (b.mTab.empty() || b.mTab != label)
			return 0;
		b.mTab.clear();
		b.mHit = true;
		return ImGuiTabItemFlags_SetSelected;
	}
}
