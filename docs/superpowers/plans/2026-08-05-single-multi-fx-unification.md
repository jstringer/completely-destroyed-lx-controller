# Single / Multi Effect Unification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Collapse "Single Fixture" and "Multiple Fixture" effects into one concept where an effect is a function of normalised position and spreads across however many sources the bound fixture group actually has.

**Architecture:** A `FixtureGroup` (ordered whole fixtures) is what a control binds to. A group flattens into an ordered **source list** per spread class — one entry per fixture for Dimmer/Strobe-type roles, one per colour unit for RGB roles (3 strobes ⇒ 3 dimmer sources, 18 colour sources). Every modulator becomes `value(pos01, component)`; a channel claim carries `(sourceIndex, sourceCount)` and evaluates on demand, so no per-voice buffer exists anywhere. Modulators split into **Field** (spatial, analytic, drives sources) and **Curve** (temporal, curve-authored, can drive a Field's inputs).

**Tech Stack:** NAP Framework 0.8.0 (C++17, RTTI-registered Resources/Components), `napsequence` curve engine, ImGui **1.76** vendored in `napimgui`, MSVC x86_64, bundled CPython buildsystem.

**Design rationale (read first, don't re-derive):** `docs/single-multi-unification-plan.md` — the audit, the eight locked decisions, and the ImGui widget reasoning. **UI reference:** Instrument UI Mockup v6, `claude.ai/code/artifact/c4591199-0c7e-4af7-9cac-bd32167a67d6`.

## Global Constraints

- **ImGui is 1.76** (`system_modules/napimgui/include/imgui/imgui.h:62`). No `imgui_internal.h`. No `BeginDisabled`, no `ImDrawFlags` (it is `ImDrawCornerFlags` in this version).
- **Every clickable button label must be ASCII and must go through `lxagent::Button` / `lxagent::SmallButton`.** `lxagent::fold()` (`src/lxagent.h:69-91`) drops all bytes ≥ 0x80, so a `✕`/`⇄`/`›`-labelled button folds to an empty, unaddressable key. Use `x`, `Rev`, `^`, `v`, `>`. Non-interactive text may use Latin-1 (`«` `»`) but nothing above U+00FF — the bundled Cascadia Code atlas uses ImGui's default glyph ranges.
- **Slabs are never nested.** `lxtheme::SlabBegin` uses one `ImDrawList::ChannelsSplit(2)`. A `Plate` goes *above* a run of sibling slabs.
- **No horizontal rules anywhere in the UI.** Separation is filled slabs, 2px gutters, and a full-height 4px spine used only for selection / collection / live state.
- **`ImGui::Combo` does not copy its items.** Keep the backing `std::vector<std::string>` alive in the same scope as the `std::vector<const char*>`.
- **GUI form state is keyed by `mID`, not pointer** (`src/lxcontrolapp.h:99`) — service mutations call `save()` and can leave a raw pointer dangling.
- **All GUI drawing happens in `lxcontrolApp::update()`**, after `mGuiService->selectWindow(mRenderWindow)` — never in `render()`.
- **Never hand-edit** `CMakeLists.txt`, `cached_app_json.cmake`, `module/cached_module_json.cmake`. Custom CMake goes in `app_extra.cmake` / `module_extra.cmake`.
- **`regenerate` is mandatory after adding any new `.cpp` to `module/src/`.** Skipping it makes `build` report success while never compiling the file; the symptom is `Unknown object type <X> encountered` at load.
- **Commit style:** conventional, scoped — `feat(engine):`, `feat(gui):`, `fix(gui):`, `refactor(engine):`, `docs:`. **Never** include `Co-Authored-By` or any AI/Claude reference in a commit message.
- **`python`, not `python3`.** Shell is Git Bash.

### Named commands (used by every task's verification)

```bash
# REGEN — after adding/removing a .cpp in module/src/
root="C:/Users/vtprodesign/Data/04 Projects/03 Misc/NAP-0.8.0-Win64-x86_64"; app="$root/apps/lxcontrol"
cd "$app" && PYTHONPATH= PYTHONHOME= "$root/thirdparty/python/msvc/x86_64/python.exe" \
  "$root/tools/buildsystem/common/regenerate_app_by_dir.py" "$app" 2>&1 | tail -40

# BUILD
root="C:/Users/vtprodesign/Data/04 Projects/03 Misc/NAP-0.8.0-Win64-x86_64"; app="$root/apps/lxcontrol"
cd "$app" && PYTHONPATH= PYTHONHOME= "$root/thirdparty/python/msvc/x86_64/python.exe" \
  "$root/tools/buildsystem/common/build_app_by_dir.py" "$app" 2>&1 | tail -40

# RUN  (background; the app holds a lock on its own exe)
cd "$app" && ./bin/Release-x86_64/lxcontrol.exe

# KILL — required before any rebuild while the app is running
cmd //c "taskkill /F /IM lxcontrol.exe"
```

- `build.bat` / `regenerate.bat` do **not** invoke from this Git Bash; call the Python entry points above.
- Run BUILD with `run_in_background: true` (it takes minutes) and then read the output: the pipe's exit code is `tail`'s, not the build's. Success looks like `lxcontrol.vcxproj -> ...lxcontrol.exe`.
- **`LNK1104: cannot open ...lxcontrol.exe`** ⇒ the app is running: KILL, then rebuild. Same error on `naplxcontrol.dll` ⇒ Napkin has the project open; close it.

### There is no unit-test framework — what replaces red/green

`CLAUDE.md`: *"There is no separate lint/test command for this app — correctness is verified by building and running."* Adding a test target would mean a new CMake target for a 6000-line app with no existing test infrastructure — out of scope. So **every task below ends with a named, observable check in the running app**, and each is chosen to fail loudly if that task's logic is wrong:

- Engine invariants are surfaced through UI readouts that already have to exist (a group's derived source counts *are* the assertion for source enumeration).
- Widget behaviour is checked in the **Style Guide test bed** (`src/lxstyleguide.cpp`, toggled from the Live Bar), which exists for exactly this.
- UI state is read back headlessly with **lx-drive**: `pwsh -NoProfile -File .claude/skills/lx-drive/scripts/lxui.ps1 <verb>` — `state` dumps mode/tab/program plus every clickable label that drew this frame; `shot -Out <path.png>` captures the window; `click "<label>"` returns HIT/MISS. Keep screenshots in the session scratchpad, never the repo.
- When judging a screenshot, judge it **aesthetically**, not just for rule-compliance.

### File structure

| File | Responsibility | Task |
|---|---|---|
| `module/src/fixturegroup.{h,cpp}` | **new** — `FixtureGroup` resource: name + ordered fixture mIDs | 1 |
| `module/src/channelrole.h` | add `ESpreadClass` + `spreadClassOf()` | 2 |
| `module/src/modulator.h` | `value(pos01, component)`, `cyclicPositions()`, `positionOf()` | 3 |
| `module/src/{chase,noise,adsr,ad,lfo,step}modulator.{h,cpp}` | adopt the new signature | 3, 7, 9 |
| `module/src/gradientmodulator.{h,cpp}` | **new** — Field modulator: Start/End colour, Phase, Period | 8 |
| `module/src/patchparameter.{h,cpp}` | authored base only — loses `mCurrentValues`, `mUnits` | 5, 6 |
| `module/src/patch.{h,cpp}` | `evaluate()` / `evaluateAt()`; two-pass `update()`; loses spread mode | 5, 7 |
| `module/src/fixturechannelcomponent.{h,cpp}` | claim carries `(patch, index, count)`; evaluates on demand | 5 |
| `module/src/trigger.h` | binding holds groups + spread flag | 6 |
| `module/src/lxcontrolservice.{h,cpp}` | group CRUD, source enumeration, `fireTrigger` over sources, persistence | 1, 2, 5, 6, 7 |
| `src/lxtheme.h` | `FieldStrip`, `InputRow` | 4, 7 |
| `src/lxstyleguide.cpp` | exercises both new helpers | 4, 7 |
| `src/lxcontrolapp.{h,cpp}` | RIG Groups editor, routing group cell, patch editor rewrite | 2, 5, 6, 7 |
| `.claude/skills/lx-drive/SKILL.md` | verb table fix | 2 |

---

### Task 1: FixtureGroup resource + persistence

**Files:**
- Create: `module/src/fixturegroup.h`, `module/src/fixturegroup.cpp`
- Modify: `module/src/lxcontrolservice.h` (includes, public API, `mGroups` member), `module/src/lxcontrolservice.cpp` (`createGroup`/`removeGroup`/`setGroupFixtures`/`ensureDefaultGroup`, the `loadUserContent` type switch, `save()`'s root-object list, `setup()`)

**Interfaces:**
- Consumes: `lxcontrolService::getFixturesPhysicalOrder()`, `makeUniqueID()`, `markDirty()` — all existing.
- Produces:
  - `lx::FixtureGroup` with `std::string mName` (RTTI `"Name"`, Required) and `std::vector<std::string> mFixtureNames` (RTTI `"Fixtures"`, Default).
  - `lx::FixtureGroup* lxcontrolService::createGroup(const std::string& name)`
  - `void lxcontrolService::removeGroup(lx::FixtureGroup* group)`
  - `void lxcontrolService::setGroupFixtures(lx::FixtureGroup& group, const std::vector<std::string>& fixtureIDs)`
  - `const std::vector<nap::rtti::ObjectPtr<lx::FixtureGroup>>& lxcontrolService::getGroups() const`
  - `lx::FixtureGroup* lxcontrolService::ensureDefaultGroup()` — returns the group named `"All Fixtures"`, creating it (populated with every fixture in physical DMX order) if absent.

- [ ] **Step 1: Create the resource header**

`module/src/fixturegroup.h`:

```cpp
#pragma once

// External Includes
#include <nap/resource.h>
#include <string>
#include <vector>

namespace lx
{
	/**
	 * An ordered set of whole fixtures — what a Control binds to. Order is meaningful: it sets the
	 * direction an effect spreads across the group (source 0 first), so reordering reverses a chase.
	 *
	 * Members are whole fixtures on purpose: a fixture's own sub-units (the Eurolite's six colour units)
	 * are declared at the fixture level and are abstracted away by the time it joins a group. The group's
	 * per-role source counts are derived from its members, never authored (see
	 * lxcontrolService::sourceCountsFor).
	 */
	class NAPAPI FixtureGroup : public nap::Resource
	{
		RTTI_ENABLE(nap::Resource)
	public:
		std::string					mName;			///< Property: 'Name'
		std::vector<std::string>	mFixtureNames;	///< Property: 'Fixtures' ordered fixture entity mIDs
	};
}
```

- [ ] **Step 2: Create the RTTI registration**

`module/src/fixturegroup.cpp`:

```cpp
#include "fixturegroup.h"

RTTI_BEGIN_CLASS(lx::FixtureGroup)
	RTTI_PROPERTY("Name",		&lx::FixtureGroup::mName,			nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("Fixtures",	&lx::FixtureGroup::mFixtureNames,	nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS
```

- [ ] **Step 3: REGEN (a new .cpp exists)**

Run REGEN. Expected: completes without error; `msvc64/` is rewritten. Skipping this makes Step 8 silently not compile the file.

- [ ] **Step 4: Add the service API declarations**

In `module/src/lxcontrolservice.h` — add `#include "fixturegroup.h"` beside the other local includes, then a public section after the `// --- Patches ---` block:

```cpp
		// --- Fixture groups (what a Control binds to; ordered whole fixtures) ---
		lx::FixtureGroup* createGroup(const std::string& name);
		void removeGroup(lx::FixtureGroup* group);
		void setGroupFixtures(lx::FixtureGroup& group, const std::vector<std::string>& fixtureIDs);
		const std::vector<rtti::ObjectPtr<lx::FixtureGroup>>& getGroups() const { return mGroups; }
		/** @return the "All Fixtures" group, creating it (every fixture, physical DMX order) if absent.
		 *  Called from setup() so a fresh install always has something bindable. */
		lx::FixtureGroup* ensureDefaultGroup();
```

and a private member beside `mPatches`:

```cpp
		std::vector<rtti::ObjectPtr<lx::FixtureGroup>>	mGroups;
```

- [ ] **Step 5: Implement the CRUD**

In `module/src/lxcontrolservice.cpp`, following the exact shape of `createProgram` (`:960-974`):

```cpp
	lx::FixtureGroup* lxcontrolService::createGroup(const std::string& name)
	{
		auto group = mResourceManager->createObject<lx::FixtureGroup>();
		group->mID = makeUniqueID("Group_" + name);
		group->mName = name;
		utility::ErrorState err;
		if (!group->init(err))
		{
			Logger::error("createGroup: %s", err.toString().c_str());
			return nullptr;
		}
		mGroups.emplace_back(group);
		markDirty();
		return group.get();
	}


	void lxcontrolService::removeGroup(lx::FixtureGroup* group)
	{
		mGroups.erase(std::remove_if(mGroups.begin(), mGroups.end(),
			[group](const rtti::ObjectPtr<lx::FixtureGroup>& g) { return g.get() == group; }), mGroups.end());
		markDirty();
	}


	void lxcontrolService::setGroupFixtures(lx::FixtureGroup& group, const std::vector<std::string>& fixtureIDs)
	{
		group.mFixtureNames = fixtureIDs;
		markDirty();
	}


	lx::FixtureGroup* lxcontrolService::ensureDefaultGroup()
	{
		for (auto& g : mGroups)
			if (g != nullptr && g->mName == "All Fixtures")
				return g.get();

		lx::FixtureGroup* group = createGroup("All Fixtures");
		if (group == nullptr)
			return nullptr;
		std::vector<std::string> ids;
		for (auto* f : getFixturesPhysicalOrder())
			ids.emplace_back(f->getEntityID());
		setGroupFixtures(*group, ids);
		return group;
	}
```

- [ ] **Step 6: Wire persistence — load**

In `loadUserContent`'s type switch (the `else if (auto* mapping = rtti_cast<lx::ControlMapping>(obj))` chain ending at `:242`), add a branch:

```cpp
			else if (auto* group = rtti_cast<lx::FixtureGroup>(obj))
				mGroups.emplace_back(rtti::ObjectPtr<lx::FixtureGroup>(group));
```

- [ ] **Step 7: Wire persistence — save**

In `save()`, add every group to the root-object list alongside the existing Patches/Triggers/Controls/Programs. **This is required, not optional:** `nap::rtti::serializeObjects` writes only what is in `rootObjects` plus what is embedded in them; a `Default`-pointer target that is not listed becomes a dangling ID on the next load.

```cpp
		for (auto& group : mGroups)
			roots.emplace_back(group.get());
```

(Match the local variable name `save()` already uses for its root list.)

- [ ] **Step 8: Create the default group at startup**

At the end of `lxcontrolService::setup()`, after the authored content has loaded:

```cpp
	// A fresh install must have something bindable, and the lifecycle rows' old "all fixtures" default
	// now needs a real group to point at.
	ensureDefaultGroup();
```

- [ ] **Step 9: BUILD**

Run KILL (if running), then BUILD. Expected: `lxcontrol.vcxproj -> ...lxcontrol.exe`, no errors.

- [ ] **Step 10: Verify persistence round-trips**

Run RUN, let it start, then KILL. Then:

```bash
grep -n "FixtureGroup" -A 6 "data/user_content.json"
```

Expected: one `lx::FixtureGroup` object with `"Name": "All Fixtures"` and a `"Fixtures"` array holding `Strobe1Entity`, `Strobe2Entity`, `Strobe3Entity` in that order. Run RUN a second time and KILL: the file must still contain exactly **one** such group (proving `ensureDefaultGroup` matched the loaded one rather than creating a duplicate).

- [ ] **Step 11: Commit**

```bash
git add module/src/fixturegroup.h module/src/fixturegroup.cpp module/src/lxcontrolservice.h module/src/lxcontrolservice.cpp
git commit -m "feat(engine): FixtureGroup resource + persistence + default All Fixtures group"
```

---

### Task 2: Source enumeration + RIG Groups editor

**Files:**
- Modify: `module/src/channelrole.h` (add `ESpreadClass`, `spreadClassOf`), `module/src/channelrole.cpp` (RTTI enum), `module/src/lxcontrolservice.h`/`.cpp` (`sourcesFor`, `sourceCountsFor`), `src/lxcontrolapp.h` (`mNewGroupName`), `src/lxcontrolapp.cpp` (`drawRigTab` gains a Groups section; `text` verb target), `.claude/skills/lx-drive/SKILL.md` (verb table)

**Interfaces:**
- Consumes: Task 1's `getGroups()`, `createGroup`, `removeGroup`, `setGroupFixtures`.
- Produces:
  - `lx::ESpreadClass { Fixture, ColourUnit }` and `lx::ESpreadClass lx::spreadClassOf(lx::EChannelRole role)`
  - `struct lx::Source { lx::FixtureComponentInstance* mFixture; int mUnitIndex; }`
  - `std::vector<lx::Source> lxcontrolService::sourcesFor(const lx::FixtureGroup& group, lx::ESpreadClass cls) const`
  - `std::vector<std::pair<lx::EChannelRole, int>> lxcontrolService::sourceCountsFor(const lx::FixtureGroup& group) const`

- [ ] **Step 1: Add the spread class**

In `module/src/channelrole.h`, after `EChannelRole`:

```cpp
	/**
	 * Which set of sources a parameter of a given role spreads over. Red/Green/Blue address one colour
	 * unit each, so a group of 3 six-unit fixtures has 18 ColourUnit sources but only 3 Fixture sources.
	 * This is the whole reason "one effect, any number of sources" works without the user declaring a count.
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
```

- [ ] **Step 2: Register the enum**

In `module/src/channelrole.cpp`, beside the existing `EChannelRole` enum registration:

```cpp
RTTI_BEGIN_ENUM(lx::ESpreadClass)
	RTTI_ENUM_VALUE(lx::ESpreadClass::Fixture,		"Fixture"),
	RTTI_ENUM_VALUE(lx::ESpreadClass::ColourUnit,	"ColourUnit")
RTTI_END_ENUM
```

- [ ] **Step 3: Declare Source + the two queries**

In `module/src/lxcontrolservice.h`, in the `namespace lx` block at the top (beside `struct Key`):

```cpp
	/** One addressable spread target: a fixture, plus which of its colour units (0 = whole-fixture roles).
	 *  Derived at fire time from a FixtureGroup; never authored, never serialized. */
	struct Source
	{
		FixtureComponentInstance*	mFixture = nullptr;
		int							mUnitIndex = 0;
	};
```

and in the service's public section, under the group block from Task 1:

```cpp
		/** Flattens a group into its ordered source list for one spread class. Fixture order follows the
		 *  group's own member order (that is what makes reordering a group reverse a chase); colour units
		 *  ascend within each fixture. Members that no longer exist in the rig are skipped. */
		std::vector<lx::Source> sourcesFor(const lx::FixtureGroup& group, lx::ESpreadClass cls) const;
		/** Per-role source counts for a group — what the RIG group row reports. Roles absent from the
		 *  group's fixtures are omitted. Red/Green/Blue each report the colour-unit count; the GUI
		 *  collapses them into one "colour" chip. */
		std::vector<std::pair<lx::EChannelRole, int>> sourceCountsFor(const lx::FixtureGroup& group) const;
```

- [ ] **Step 4: Implement both queries**

In `module/src/lxcontrolservice.cpp`:

```cpp
	std::vector<lx::Source> lxcontrolService::sourcesFor(const lx::FixtureGroup& group, lx::ESpreadClass cls) const
	{
		std::vector<lx::Source> sources;
		for (const std::string& id : group.mFixtureNames)
		{
			lx::FixtureComponentInstance* fixture = findFixture(id);
			if (fixture == nullptr)
				continue;			// group outlived a fixture; skip rather than shifting every position

			if (cls == lx::ESpreadClass::Fixture)
			{
				sources.push_back({ fixture, 0 });
				continue;
			}

			// Colour units, ascending, deduplicated: three channels (R/G/B) share one unit index.
			std::vector<int> units;
			for (auto* channel : fixture->getChannels())
			{
				if (lx::spreadClassOf(channel->getRole()) != lx::ESpreadClass::ColourUnit)
					continue;
				if (std::find(units.begin(), units.end(), channel->getUnitIndex()) == units.end())
					units.emplace_back(channel->getUnitIndex());
			}
			std::sort(units.begin(), units.end());
			for (int u : units)
				sources.push_back({ fixture, u });
		}
		return sources;
	}


	std::vector<std::pair<lx::EChannelRole, int>> lxcontrolService::sourceCountsFor(const lx::FixtureGroup& group) const
	{
		const size_t fixture_sources = sourcesFor(group, lx::ESpreadClass::Fixture).size();
		const size_t colour_sources  = sourcesFor(group, lx::ESpreadClass::ColourUnit).size();

		// Only report roles the group's fixtures actually have.
		std::vector<std::pair<lx::EChannelRole, int>> counts;
		std::vector<lx::EChannelRole> seen;
		for (const std::string& id : group.mFixtureNames)
		{
			lx::FixtureComponentInstance* fixture = findFixture(id);
			if (fixture == nullptr)
				continue;
			for (auto* channel : fixture->getChannels())
			{
				const lx::EChannelRole role = channel->getRole();
				if (std::find(seen.begin(), seen.end(), role) != seen.end())
					continue;
				seen.emplace_back(role);
				const bool colour = lx::spreadClassOf(role) == lx::ESpreadClass::ColourUnit;
				counts.emplace_back(role, static_cast<int>(colour ? colour_sources : fixture_sources));
			}
		}
		return counts;
	}
```

- [ ] **Step 5: Add the group-name form buffer**

In `src/lxcontrolapp.h`, beside `mNewProgramName` (the mID-keyed form-state block at `:99`):

```cpp
		char	mNewGroupName[64] = { 0 };		///< RIG > Groups creation form
```

- [ ] **Step 6: Fix the lx-drive text verb (it currently collides)**

`src/lxcontrolapp.cpp:146` maps `text group` to `mNewControlGroup`, which is a **Control's device label**, not a fixture group. Rename that target to match its visible UI label (`"Device##ctrl"`, `:1487`) and give `group` to fixture groups:

```cpp
			else if (field == "device")		{ dst = mNewControlGroup;	cap = sizeof(mNewControlGroup); }
			else if (field == "group")		{ dst = mNewGroupName;		cap = sizeof(mNewGroupName); }
```

Then update the verb table in `.claude/skills/lx-drive/SKILL.md`:

```
| `text patch\|control\|device\|group\|program <value>` | fill a creation-form text buffer |
```

- [ ] **Step 7: Draw the Groups section in RIG**

In `src/lxcontrolapp.cpp`'s RIG tab drawing, after the existing fixture strips. Slabs are siblings under one plate (never nested), alternating fill, teal spine because a group is a selectable/bindable collection:

```cpp
	static const char* kRoleShort[] = { "dimmer", "strobe", "colour", "colour", "colour", "macro", "sound", "generic" };

	lxtheme::Plate("Groups", lxtheme::accent());
	ImGui::SameLine(); lxtheme::Chip("what a control binds to");
	ImGui::SameLine(); lxtheme::Chip("order sets the spread direction", lxtheme::mod2());

	lx::FixtureGroup* group_to_remove = nullptr;
	int gi = 0;
	for (auto& group : mLxControlService->getGroups())
	{
		ImGui::PushID(group->mID.c_str());
		lxtheme::SlabBegin((gi & 1) ? lxtheme::slab2() : lxtheme::slab(), lxtheme::accent());

		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(lxtheme::text(), "%s", group->mName.c_str());

		// Ordered members. Index is shown because order IS semantics here (spread direction).
		std::vector<std::string> members = group->mFixtureNames;
		int move_up = -1, remove_at = -1;
		for (int mi = 0; mi < static_cast<int>(members.size()); ++mi)
		{
			lx::FixtureComponentInstance* fx = nullptr;
			for (auto* f : mLxControlService->getFixturesPhysicalOrder())
				if (f->getEntityID() == members[mi]) { fx = f; break; }

			ImGui::SameLine();
			ImGui::PushID(mi);
			lxtheme::Chip((std::to_string(mi + 1) + " " + (fx != nullptr ? fx->getDisplayName() : "(missing)")).c_str());
			if (mi > 0)
			{
				ImGui::SameLine(0.0f, 2.0f);
				if (lxagent::SmallButton("^")) move_up = mi;		// addressable as ^, ^#2, ...
			}
			ImGui::SameLine(0.0f, 2.0f);
			if (lxagent::SmallButton("x")) remove_at = mi;
			ImGui::PopID();
		}
		if (move_up > 0)
		{
			std::swap(members[move_up - 1], members[move_up]);
			mLxControlService->setGroupFixtures(*group, members);
		}
		else if (remove_at >= 0)
		{
			members.erase(members.begin() + remove_at);
			mLxControlService->setGroupFixtures(*group, members);
		}

		// Add any fixture not already a member (a combo, so the agent bridge can't drive it — by design,
		// the bridge only reaches buttons).
		{
			std::vector<lx::FixtureComponentInstance*> avail;
			std::vector<std::string> labels;
			for (auto* f : mLxControlService->getFixturesPhysicalOrder())
				if (std::find(members.begin(), members.end(), f->getEntityID()) == members.end())
					{ avail.emplace_back(f); labels.emplace_back(f->getDisplayName()); }
			if (!avail.empty())
			{
				std::vector<const char*> items; items.emplace_back("+ fixture");
				for (auto& s : labels) items.emplace_back(s.c_str());	// labels must outlive this Combo call
				int sel = 0;
				ImGui::SameLine(); ImGui::SetNextItemWidth(110.0f);
				if (ImGui::Combo("##addfx", &sel, items.data(), static_cast<int>(items.size())) && sel > 0)
				{
					members.emplace_back(avail[sel - 1]->getEntityID());
					mLxControlService->setGroupFixtures(*group, members);
				}
			}
		}

		ImGui::SameLine();
		if (lxagent::SmallButton("Rev"))
		{
			std::reverse(members.begin(), members.end());
			mLxControlService->setGroupFixtures(*group, members);
		}
		ImGui::SameLine();
		if (lxtheme::DangerButton("Del")) group_to_remove = group.get();

		// Derived counts — the ONLY place in the app that states a source count (that and the rig's own
		// per-fixture readout). R/G/B collapse into one "colour" chip.
		bool colour_shown = false;
		for (auto& rc : mLxControlService->sourceCountsFor(*group))
		{
			const bool colour = lx::spreadClassOf(rc.first) == lx::ESpreadClass::ColourUnit;
			if (colour && colour_shown)
				continue;
			colour_shown |= colour;
			ImGui::SameLine();
			lxtheme::Chip((std::to_string(rc.second) + " " +
				kRoleShort[nap::math::clamp(static_cast<int>(rc.first), 0, 7)]).c_str(), lxtheme::mod2());
		}

		lxtheme::SlabEnd();
		ImGui::PopID();
		++gi;
	}
	if (group_to_remove != nullptr) mLxControlService->removeGroup(group_to_remove);

	ImGui::SetNextItemWidth(-140.0f);
	ImGui::InputText("##newgroup", mNewGroupName, sizeof(mNewGroupName));
	ImGui::SameLine();
	if (lxagent::Button("+ New group") && std::strlen(mNewGroupName) > 0)
	{
		mLxControlService->createGroup(mNewGroupName);
		mNewGroupName[0] = '\0';
	}
```

- [ ] **Step 8: BUILD**

KILL if running, then BUILD. Expected: no errors. (No REGEN needed — no new `.cpp`.)

- [ ] **Step 9: Verify the derived counts — this is the assertion for source enumeration**

Run RUN, then:

```bash
pwsh -NoProfile -File .claude/skills/lx-drive/scripts/lxui.ps1 tab RIG
pwsh -NoProfile -File .claude/skills/lx-drive/scripts/lxui.ps1 text group "Outer Pair"
pwsh -NoProfile -File .claude/skills/lx-drive/scripts/lxui.ps1 click "+ New group"
pwsh -NoProfile -File .claude/skills/lx-drive/scripts/lxui.ps1 shot -Out "<scratchpad>/rig-groups.png"
```

Read the PNG. Expected, and each of these fails visibly if `sourcesFor` is wrong:
- **All Fixtures** reports `3 dimmer`, `3 strobe`, `18 colour` (3 fixtures × 6 units), plus `1 macro` / `1 sound`… — note macro/sound are Fixture class so they read `3`.
- **Outer Pair** starts empty: `0 …` and no chips. Add Strobe 1 and Strobe 3 via the `+ fixture` combo (click in the window, or verify by screenshot after using the combo manually) ⇒ `2 dimmer`, `2 strobe`, `12 colour`.
- `Rev` on All Fixtures swaps the member index chips to `1 Strobe 3 … 3 Strobe 1` and the counts do **not** change.
- Judge the layout aesthetically: chips should not wrap raggedly, and the count chips should sit clear of the member chips.

- [ ] **Step 10: Verify the verb rename didn't break control creation**

```bash
pwsh -NoProfile -File .claude/skills/lx-drive/scripts/lxui.ps1 tab CONTROLS
pwsh -NoProfile -File .claude/skills/lx-drive/scripts/lxui.ps1 text device "APC mini"
pwsh -NoProfile -File .claude/skills/lx-drive/scripts/lxui.ps1 state
```

Expected: `RESULT: HIT` on the `text device` command.

- [ ] **Step 11: Commit**

```bash
git add module/src/channelrole.h module/src/channelrole.cpp module/src/lxcontrolservice.h module/src/lxcontrolservice.cpp src/lxcontrolapp.h src/lxcontrolapp.cpp .claude/skills/lx-drive/SKILL.md
git commit -m "feat(engine): source enumeration by spread class + RIG Groups editor"
```

---

### Task 3: Position-based modulator evaluation

**Files:**
- Modify: `module/src/modulator.h` (signature, `cyclicPositions`, `positionOf`), `module/src/modulator.cpp` (base `value`), `module/src/chasemodulator.{h,cpp}`, `module/src/noisemodulator.{h,cpp}`, `src/lxcontrolapp.cpp:968-980` (the one GUI call site), `module/src/patch.cpp` (`update`'s call site)

**Interfaces:**
- Consumes: nothing new.
- Produces:
  - `virtual float lx::Modulator::value(float pos01, int component) const` — replaces `value()` and `valueForVoice(int)`. Base ignores both arguments and returns the sink mapped to `[mMin, mMax]`.
  - `virtual bool lx::Modulator::cyclicPositions() const` — `false` by default; `ChaseModulator` overrides to `true`.
  - `float lx::positionOf(const lx::Modulator& m, int index, int count)` — free function in `modulator.h`.
- **Removed:** `valueForVoice(int)`, `setVoiceCount(int)`, `ChaseModulator::mVoiceCount`, `NoiseModulator::mVoiceCount`, `NoiseModulator::mSeed`.

**Why `cyclicPositions` exists — do not "simplify" it away:** a chase across N fixtures needs offsets `0, 1/N, … (N-1)/N`, or the wrap seam lands on source 0 and the last source duplicates the first (this is exactly what today's `voice / mVoiceCount` at `chasemodulator.cpp:61` does). A gradient across N sources needs `0 … 1` inclusive, or its End colour is never reached. One convention cannot serve both, so the modulator declares which it wants.

- [ ] **Step 1: Change the base signature**

In `module/src/modulator.h`, replace the `value()` / `valueForVoice()` / `setVoiceCount()` trio:

```cpp
		/**
		 * @return this modulator's output at normalised position `pos01` for value component `component`.
		 * The base implementation ignores both and returns the sink mapped to [Min,Max] — i.e. Curve
		 * modulators are identical on every source. Field modulators (Chase/Noise/Gradient) override.
		 *
		 * `component` is the component of the VALUE (0..2 of an RGB colour), used by fields that
		 * decorrelate per channel. It is NOT mTargetComponent, which selects which components get
		 * written — the two are independent and easy to confuse.
		 */
		virtual float value(float pos01, int component) const;

		/** @return true when this modulator's source positions should be endpoint-EXCLUSIVE (i/n), so a
		 *  looping shape's seam does not land on source 0. Chase: true. Gradients/envelopes: false (i/(n-1),
		 *  so the last source reaches the end of the shape). See positionOf(). */
		virtual bool cyclicPositions() const		{ return false; }
```

In `module/src/modulator.cpp`, rename the existing `value()` body to `value(float, int)` and ignore both parameters (the mapping to `[mMin, mMax]` is unchanged):

```cpp
	float Modulator::value(float /*pos01*/, int /*component*/) const
	{
		// unchanged body: read mSink, map 0..1 into [mMin, mMax]
	}
```

- [ ] **Step 2: Add positionOf**

At the end of `module/src/modulator.h`, inside `namespace lx`:

```cpp
	/** @return the normalised position of source `index` of `count`, using the modulator's own convention
	 *  (see Modulator::cyclicPositions). count <= 1 collapses to 0 — which is the case that used to be
	 *  called "Single" mode. */
	inline float positionOf(const Modulator& m, int index, int count)
	{
		if (count <= 1)
			return 0.0f;
		return m.cyclicPositions() ? static_cast<float>(index) / count
		                           : static_cast<float>(index) / (count - 1);
	}
```

- [ ] **Step 3: Port ChaseModulator**

`module/src/chasemodulator.h`: delete `setVoiceCount` and the private `mVoiceCount`; declare the new overrides.

```cpp
		float value(float pos01, int component) const override;
		bool cyclicPositions() const override		{ return true; }	// keeps the wrap seam off source 0
```

`module/src/chasemodulator.cpp`: replace `valueForVoice`'s body — the position maths was always position maths, it just spelled the position as `voice / mVoiceCount`:

```cpp
	float ChaseModulator::value(float pos01, int /*component*/) const
	{
		if (mPlayer == nullptr)
			return 0.0f;

		double phase = wrapFrac(mPlayer->getPlayerTime() - static_cast<double>(pos01));
		float pw = nap::math::clamp(mPulseWidth, 0.01f, 1.0f);

		if (!mGlide)
			return phase < pw ? 1.0f : 0.0f;

		// unchanged soft-edge branch
		double edge = std::min(0.15 * pw, 0.05);
		if (phase < edge)       return static_cast<float>(phase / edge);
		if (phase < pw - edge)  return 1.0f;
		if (phase < pw)         return static_cast<float>((pw - phase) / edge);
		return 0.0f;
	}
```

- [ ] **Step 4: Port NoiseModulator and delete its seed**

`module/src/noisemodulator.h`: delete `setVoiceCount`, `mVoiceCount`, **and `mSeed`** — decorrelation now comes from `component`, which is what `mSeed` was manually working around ("one per R/G/B component"). Declare `float value(float pos01, int component) const override;`.

`module/src/noisemodulator.cpp`: remove the `RTTI_PROPERTY("Seed", ...)` line, and quantise position into the hash so each source still holds its own value:

```cpp
	float NoiseModulator::value(float pos01, int component) const
	{
		double t = mElapsed * static_cast<double>(std::max(mRate, 0.001f));
		double step_d = std::floor(t);
		uint64_t step = static_cast<uint64_t>(std::max(step_d, 0.0));
		float frac = static_cast<float>(t - step_d);

		// Position bucket stands in for the old voice index; `component` replaces the per-instance seed,
		// so ONE Noise on a Colour parameter decorrelates R/G/B by itself.
		const int bucket = static_cast<int>(pos01 * 1024.0f);
		float a = hash01(bucket, step, component);
		if (mSmoothing <= 0.0f)
			return a;
		float b = hash01(bucket, step + 1, component);
		return nap::math::lerp(a, b, smoothstep01(frac));
	}
```

Also delete the `mNextNoiseSeed` member from `lxcontrolservice.h`, its auto-assignment in `addModulator`, and the `mNextNoiseSeed = std::max(...)` line in `rewireModulator` (`lxcontrolservice.cpp:286-287`).

> Spatial *coherence* (Density, Perlin-ish blending between neighbours) is **Task 9**. This step only ports the existing look onto the new signature: buckets are still independent, exactly as voices were.

- [ ] **Step 5: Update Patch::update's call site (transitional)**

`module/src/patch.cpp:56-58` — the loop still uses `voices` from `mFixtureCount` at this point (Task 5 removes that). Feed it a position:

```cpp
					for (int s = 0; s < voices; ++s)
					{
						const float pos = positionOf(*modulator, s, voices);
						for (int c = from; c <= to && c < count; ++c)
						{
							float v = modulator->value(pos, c);
							// unchanged blend switch, then target->setComponentValue(s, c, blended);
						}
					}
```

Note the `value()` call moves *inside* the component loop — a field may differ per component now.

- [ ] **Step 6: Update the one GUI call site**

`src/lxcontrolapp.cpp:976` currently calls `m->valueForVoice(s)`. Replace with the position form (this whole block is deleted in Task 5; keep it compiling for now):

```cpp
							ImGui::ProgressBar(m->value(positionOf(*m, s, voices), 0), ImVec2(90, 0),
								describePatchVoice(patch.get(), s).c_str());
```

- [ ] **Step 7: BUILD**

KILL, BUILD. Expected: no errors, and **no remaining references** to the deleted names:

```bash
grep -rn "valueForVoice\|setVoiceCount\|mVoiceCount\|mNextNoiseSeed\|mSeed" module/src src
```

Expected: no hits.

- [ ] **Step 8: Verify the chase still behaves**

Delete `data/user_content.json` (the `Seed` property is gone, so an old file no longer resolves), run RUN, then build a patch by hand in the GUI: a Dimmer source, a Chase modulator on it, spread Multiple, routed to all 3 fixtures. Fire it.

Expected: the three fixtures pulse **in sequence, one after another**, and fixture 3 is distinct from fixture 1. If `cyclicPositions()` were wrong (returning `false`), fixtures 1 and 3 would flash together — that is the observable this step exists to catch. Confirm in the RIG tab's live dimmer faders, or `lxui.ps1 shot`.

- [ ] **Step 9: Commit**

```bash
git add module/src
git commit -m "refactor(engine): modulators evaluate at (pos01, component); drop voice counts + noise seed"
```

---

### Task 4: `lxtheme::FieldStrip`

**Files:**
- Modify: `src/lxtheme.h` (new helper), `src/lxstyleguide.cpp` (exercise it)

**Interfaces:**
- Consumes: nothing.
- Produces: `void lxtheme::FieldStrip(const std::function<float(float pos01, int component)>& field, int components, const ImVec2& size_arg)`

- [ ] **Step 1: Add the helper**

In `src/lxtheme.h`, after `PlayheadPreview` (add `#include <functional>` and `#include <algorithm>` at the top):

```cpp
	/** Horizontal field strip: a modulator (or a patch parameter's resolved field) sampled as a continuous
	 *  function of position 0..1 — the readout that replaces the per-voice meter row. `components` >= 3 ->
	 *  RGB; otherwise the gold luminance ramp (a float field must not invent a fourth hue).
	 *
	 *  One sample per PIXEL COLUMN, deliberately: the fields that matter most are hard-edged (Chase's
	 *  pulse, Noise at Smoothing 0), and coarser sampling plus interpolation smears those edges or misses
	 *  them outright — a Chase at PulseWidth 0.05 can vanish between samples. Per-column is also the
	 *  simplest thing that works: no segment count to tune, no interpolation mode to pick.
	 *  ponytail: if this ever profiles hot the upgrade is PrimReserve + direct vertex writes, NOT a
	 *  coarser sample count — coarser sampling buys frames by drawing something the rig isn't doing. */
	inline void FieldStrip(const std::function<float(float pos01, int component)>& field,
	                       int components, const ImVec2& size_arg)
	{
		ImVec2 size = size_arg;
		if (size.x <= 0.0f) size.x = ImGui::GetContentRegionAvail().x;	// -1 => fill width, not raw -1px
		if (size.y <= 0.0f) size.y = 28.0f;

		const ImVec2 p = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const int cols = std::max(2, static_cast<int>(size.x));

		for (int i = 0; i < cols; ++i)
		{
			const float pos = static_cast<float>(i) / (cols - 1);
			ImU32 col;
			if (components >= 3)
			{
				col = ImGui::ColorConvertFloat4ToU32(ImVec4(
					std::min(std::max(field(pos, 0), 0.0f), 1.0f),
					std::min(std::max(field(pos, 1), 0.0f), 1.0f),
					std::min(std::max(field(pos, 2), 0.0f), 1.0f), 1.0f));
			}
			else
			{
				const float v = std::min(std::max(field(pos, 0), 0.0f), 1.0f);
				const ImVec4 g = live();
				col = ImGui::ColorConvertFloat4ToU32(ImVec4(g.x * v, g.y * v, g.z * v, 1.0f));
			}
			const float x0 = p.x + size.x * i / cols;
			const float x1 = p.x + size.x * (i + 1) / cols;
			dl->AddRectFilled(ImVec2(x0, p.y), ImVec2(x1 + 0.5f, p.y + size.y), col);	// .5: no sub-pixel seams
		}
		ImGui::Dummy(size);
	}
```

- [ ] **Step 2: Exercise it in the Style Guide**

In `src/lxstyleguide.cpp`, add a section. The hard-edged case is the point of the test, so include it:

```cpp
	ImGui::Spacing();
	lxtheme::Plate("Field strip", lxtheme::mod());

	// 1. Smooth RGB gradient — blue to gold.
	lxtheme::FieldStrip([](float pos, int c) {
		const float s[3] = { 0.0f, 0.22f, 1.0f }, e[3] = { 0.96f, 0.70f, 0.0f };
		return s[c] + (e[c] - s[c]) * pos;
	}, 3, ImVec2(-1.0f, 30.0f));

	// 2. Float field — gold luminance ramp.
	lxtheme::FieldStrip([](float pos, int) { return pos; }, 1, ImVec2(-1.0f, 20.0f));

	// 3. THE REGRESSION CASE: a 0.05-wide hard pulse. It must render as a thin, crisp, clearly visible
	//    band. If it is soft-edged or invisible, the strip is interpolating or under-sampling.
	lxtheme::FieldStrip([](float pos, int) { return pos < 0.05f ? 1.0f : 0.0f; }, 1, ImVec2(-1.0f, 20.0f));
```

- [ ] **Step 3: BUILD**

KILL, BUILD. Expected: no errors.

- [ ] **Step 4: Verify in the Style Guide — this is the assertion for per-column sampling**

```bash
pwsh -NoProfile -File .claude/skills/lx-drive/scripts/lxui.ps1 styleguide on
pwsh -NoProfile -File .claude/skills/lx-drive/scripts/lxui.ps1 shot -Out "<scratchpad>/fieldstrip.png"
```

Read the PNG. Expected:
- Strip 1 is a smooth blue→gold gradient with no visible banding or seams.
- Strip 2 is black at the left, full gold at the right, no hue shift.
- **Strip 3 shows a crisp gold band roughly 1/20th of the strip width, with a hard right edge.** A soft or absent band means the sampling regressed.

- [ ] **Step 5: Commit**

```bash
git add src/lxtheme.h src/lxstyleguide.cpp
git commit -m "feat(gui): FieldStrip - per-column field readout + Style Guide cases"
```

---

### Task 5: Claims evaluate on demand; delete the spread mode

This is the structural task. It is atomic because deleting `PatchParameter::mCurrentValues` breaks the claim resolver and the patch editor in the same compile.

**Files:**
- Modify: `module/src/patchparameter.h`/`.cpp` (remove `mCurrentValues` + accessors + `resetToBase`), `module/src/patch.h`/`.cpp` (`evaluate`/`evaluateAt`; remove spread mode; `update` becomes transport-only), `module/src/fixturechannelcomponent.h`/`.cpp` (claim carries patch + index + count), `module/src/lxcontrolservice.h`/`.cpp` (`fireTrigger` pushes index/count; delete `syncPatchFixtureCount`, `setPatchTargetMode`), `src/lxcontrolapp.h`/`.cpp` (delete the spread UI, per-voice loops and `describePatchVoice`; draw `FieldStrip`)

**Interfaces:**
- Consumes: Task 3's `lx::positionOf`, `Modulator::value(pos01, component)`; Task 4's `lxtheme::FieldStrip`.
- Produces:
  - `float lx::Patch::evaluate(const lx::PatchParameter& param, int sourceIndex, int sourceCount, int component) const` — the authored base blended with every modulator targeting `param`, each modulator evaluated at **its own** `positionOf(m, sourceIndex, sourceCount)`. Clamped 0..1.
  - `float lx::Patch::evaluateAt(const lx::PatchParameter& param, float pos01, int component) const` — same blend, but every modulator is evaluated at the given continuous position. For previews (`FieldStrip`), which have no source count.
  - `void lx::FixtureChannelComponentInstance::pushClaim(uint64_t activationId, const lx::Patch* patch, const lx::PatchParameter* param, int component, int sourceIndex, int sourceCount, bool held)`
- **Removed:** `PatchParameter::mCurrentValues`, `getComponentValue`, `setComponentValue`, `resetToBase`; `Patch::mTargetMode`, `Patch::mFixtureCount`, `EPatchTargetMode`; `lxcontrolService::setPatchTargetMode`, `syncPatchFixtureCount`; `lxcontrolApp::describePatchVoice`.

**Why there is no per-activation buffer:** the claim already knows `(patch, param, component, index, count)`, so the channel can call `evaluate()` in `resolveValue()` and hold no state at all. That deletes the shared-buffer collision two activations of one patch have today, and deletes the storage question with it. *ponytail: ~66 evaluate() calls per frame (3 fixtures × 22 channels), each looping a handful of modulators. Memoize per (patch, index) only if a profile ever says so.*

- [ ] **Step 1: Strip PatchParameter to authored base only**

`module/src/patchparameter.h`: delete `mCurrentValues`, both `getComponentValue` overloads, both `setComponentValue` overloads, `resetToBase`, and `appliesToUnit`/`mUnits` **stay for now** (Task 6 removes them with their caller). Keep `getComponentCount`, `getComponentRole`, `getBaseValue`, `init`.

`module/src/patchparameter.cpp`: `init` becomes `return true;`. Delete the removed bodies. Leave the RTTI blocks alone except that `"Units"` stays until Task 6.

- [ ] **Step 2: Give Patch the two evaluators**

`module/src/patch.h`: delete `EPatchTargetMode`, `mTargetMode`, `mFixtureCount`. Declare:

```cpp
		/** @return `param`'s component `component` for source `sourceIndex` of `sourceCount`: the authored
		 *  base with every modulator that targets `param` blended in, each evaluated at its own position
		 *  (see positionOf). Stateless — this is what a channel claim calls each frame. */
		float evaluate(const PatchParameter& param, int sourceIndex, int sourceCount, int component) const;

		/** Same blend, every modulator evaluated at the continuous position `pos01`. For previews
		 *  (lxtheme::FieldStrip), which draw the field itself and so have no source count. */
		float evaluateAt(const PatchParameter& param, float pos01, int component) const;
```

`module/src/patch.cpp`: delete the enum registration and both RTTI property lines for `TargetMode`/`FixtureCount`. Replace `update()`'s value computation with the two evaluators plus a transport-only `update()`:

```cpp
	void Patch::update(double deltaTime)
	{
		// Transport housekeeping only. Values are no longer precomputed anywhere — a channel claim calls
		// evaluate() when it needs one, so nothing here depends on how many sources exist.
		for (auto& modulator : mModulators)
			modulator->update(deltaTime);
	}


	float Patch::blend(const PatchParameter& param, int component, float pos01, bool usePosition,
	                   int index, int count) const
	{
		float v = nap::math::clamp(param.getBaseValue(component), 0.0f, 1.0f);
		for (auto& modulator : mModulators)
		{
			bool targets = false;
			for (auto& t : modulator->mTargets)
				if (t.get() == &param) { targets = true; break; }
			if (!targets)
				continue;

			// mTargetComponent selects WHICH components this modulator writes (-1 = all).
			if (modulator->mTargetComponent >= 0 && modulator->mTargetComponent != component)
				continue;

			const float pos = usePosition ? pos01 : positionOf(*modulator, index, count);
			const float m = modulator->value(pos, component);
			switch (modulator->mBlend)
			{
			case EModulatorBlend::Replace:	v = m;			break;
			case EModulatorBlend::Multiply:	v = v * m;		break;
			case EModulatorBlend::Add:		v = v + m;		break;
			}
			v = nap::math::clamp(v, 0.0f, 1.0f);
		}
		return v;
	}


	float Patch::evaluate(const PatchParameter& param, int sourceIndex, int sourceCount, int component) const
	{
		return blend(param, component, 0.0f, false, sourceIndex, sourceCount);
	}


	float Patch::evaluateAt(const PatchParameter& param, float pos01, int component) const
	{
		return blend(param, component, pos01, true, 0, 1);
	}
```

Declare `blend` as a private member of `Patch` with that exact signature.

- [ ] **Step 3: Rework the channel claim**

`module/src/fixturechannelcomponent.h` — the struct and the push signature:

```cpp
		struct ChannelClaim
		{
			uint64_t				mActivationId = 0;
			const Patch*			mPatch = nullptr;
			const PatchParameter*	mParam = nullptr;
			int						mComponent = 0;
			int						mSourceIndex = 0;
			int						mSourceCount = 1;
			bool					mHeld = false;
		};
```

```cpp
		/** Adds/replaces the claim for the given activation. `sourceIndex`/`sourceCount` locate this channel
		 *  in the fired group's source list; the value itself is evaluated on demand in resolveValue(). */
		void pushClaim(uint64_t activationId, const Patch* patch, const PatchParameter* param,
		               int component, int sourceIndex, int sourceCount, bool held);
```

`module/src/fixturechannelcomponent.cpp` — the winner-selection logic is **unchanged** (held-priority LTP); only the final read changes:

```cpp
		if (winner != nullptr && winner->mPatch != nullptr && winner->mParam != nullptr)
			return nap::math::clamp(winner->mPatch->evaluate(*winner->mParam, winner->mSourceIndex,
				winner->mSourceCount, winner->mComponent), 0.0f, 1.0f);
		return nap::math::clamp(mBaseParameter->mValue, 0.0f, 1.0f);
```

and `pushClaim` stores the new fields (the `removeClaims` + `push_back` ordering is unchanged).

- [ ] **Step 4: Update fireTrigger (transitional — still fixture-name based)**

Task 6 replaces the fixture loop with groups. For now keep `binding.mFixtureNames` but pass index/count, and delete `syncPatchFixtureCount`. In `lxcontrolservice.cpp:806-827`:

```cpp
				const int source_count = matched_count;		// per-fixture for now; Task 6 makes it per-source
				int source_index = 0;
				for (auto* fixture : ordered_fixtures)
				{
					if (std::find(binding.mFixtureNames.begin(), binding.mFixtureNames.end(),
						fixture->getEntityID()) == binding.mFixtureNames.end())
						continue;

					for (auto* channel : fixture->getChannels())
						for (auto& param : patch->mParameters)
							for (int c = 0; c < param->getComponentCount(); ++c)
								if (param->getComponentRole(c) == channel->getRole() && param->appliesToUnit(channel->getUnitIndex()))
									channel->pushClaim(activation.mId, patch, param.get(), c,
										source_index, source_count, held);
					++source_index;
				}
```

Delete `syncPatchFixtureCount` (declaration `lxcontrolservice.h:219`, body `:590-606`) and `setPatchTargetMode` (`:95`, `:570-580`), plus the `setVoiceCount` re-propagation lines in `rewireModulator` (`:279-282`) and `addModulator` (`:519`).

- [ ] **Step 5: Delete the spread UI and per-voice widgets**

In `src/lxcontrolapp.cpp`:
- Delete the Spread combo + voice chip (`:779-791`).
- Replace the whole per-voice `Current` block (`:797-849`) with one `FieldStrip` per parameter:

```cpp
					if (!patch->mParameters.empty())
					{
						lxtheme::Plate("Current", lxtheme::muted());
						lxtheme::SlabBegin(lxtheme::slab(), lxtheme::muted());
						for (auto& p : patch->mParameters)
						{
							ImGui::PushID(p.get());
							ImGui::PushStyleColor(ImGuiCol_Text, lxtheme::muted());
							ImGui::TextUnformatted(patchParamLabel(p.get()).c_str());
							ImGui::PopStyleColor();
							lx::Patch* pt = patch.get();
							lx::PatchParameter* pp = p.get();
							lxtheme::FieldStrip([pt, pp](float pos, int c) { return pt->evaluateAt(*pp, pos, c); },
								pp->getComponentCount(), ImVec2(-1.0f, 30.0f));
							ImGui::PopID();
						}
						lxtheme::SlabEnd();
					}
```

- Replace the `is_voice_mod` per-voice `ProgressBar` block (`:968-980`) with a `FieldStrip` of that modulator alone, keeping the `PlayheadPreview` branch for the others:

```cpp
						if (is_voice_mod)
						{
							lx::Modulator* mm = m.get();
							lxtheme::FieldStrip([mm](float pos, int c) { return mm->value(pos, c); },
								targetComponentCount(m.get()), ImVec2(-1.0f, 26.0f));
						}
```

Add a small local helper `targetComponentCount(lx::Modulator*)` returning the max `getComponentCount()` across `mTargets` (3 for a colour target, else 1), defaulting to 1 when there are no targets.

- Delete `describePatchVoice` entirely: declaration `src/lxcontrolapp.h:74-78`, body `src/lxcontrolapp.cpp:674-695`.

- [ ] **Step 6: BUILD**

KILL, BUILD. Expected: no errors, and:

```bash
grep -rn "mCurrentValues\|mTargetMode\|mFixtureCount\|EPatchTargetMode\|describePatchVoice\|syncPatchFixtureCount\|setPatchTargetMode\|resetToBase" module/src src
```

Expected: no hits.

- [ ] **Step 7: Delete the stale content file**

```bash
rm -f data/user_content.json
```

`TargetMode` / `FixtureCount` / `Seed` no longer exist as properties, so an old file cannot resolve. Per the standing "user_content is placeholder" directive there is no migration; the default group is recreated by Task 1's `ensureDefaultGroup`.

- [ ] **Step 8: Verify the field readout and LTP**

Run RUN. Author, in the GUI: a patch with a Colour source and a Noise modulator on it; a Control; route the control to the patch on all 3 fixtures; load the program; fire it.

Expected:
- The patch editor's **Current** row is a single animated RGB strip — no row of per-fixture swatches anywhere, and no "3 voices" chip.
- RIG shows all three fixtures' colour units lit and *differing from each other* (source index still reaches the claim).
- Holding a second control that claims the same channels takes over, and releasing it hands back — held-priority LTP unchanged.
- `lxui.ps1 state` reports a nonzero voice count while held.

- [ ] **Step 9: Commit**

```bash
git add module/src src
git commit -m "refactor(engine): claims evaluate on demand; delete spread mode + per-voice storage"
```

---

### Task 6: Bindings hold groups

**Files:**
- Modify: `module/src/trigger.h` (binding shape), `module/src/patchparameter.h`/`.cpp` (delete `mUnits`/`appliesToUnit`), `module/src/lxcontrolservice.h`/`.cpp` (`fireTrigger` over source lists; `setRoutingGroups`, `setRoutingSpread`; `routeControl` signature), `src/lxcontrolapp.cpp` (routing group cell, Automatic rows, Perform sublabels)

**Interfaces:**
- Consumes: Task 2's `sourcesFor`, `spreadClassOf`, `lx::Source`; Task 5's `pushClaim`.
- Produces:
  - `lx::PatchFixtureBinding { nap::ResourcePtr<lx::Patch> mPatch; std::vector<nap::ResourcePtr<lx::FixtureGroup>> mGroups; bool mEndToEnd = false; }`
  - `void lxcontrolService::setRoutingGroups(lx::Trigger& trigger, const std::vector<nap::ResourcePtr<lx::FixtureGroup>>& groups)`
  - `void lxcontrolService::setRoutingSpread(lx::Trigger& trigger, bool endToEnd)`
  - `lx::ControlMapping* lxcontrolService::routeControl(lx::Program& program, lx::Control& control, lx::Patch* patch, const std::vector<nap::ResourcePtr<lx::FixtureGroup>>& groups)`
- **Removed:** `PatchFixtureBinding::mFixtureNames`, `lxcontrolService::setRoutingFixtures`, `PatchParameter::mUnits`, `PatchParameter::appliesToUnit`.

- [ ] **Step 1: Reshape the binding**

`module/src/trigger.h`:

```cpp
	/**
	 * Binds a Patch to one or more FixtureGroups. When the owning Trigger fires, the patch claims the
	 * matching channels of every source in those groups.
	 */
	struct NAPAPI PatchFixtureBinding
	{
		nap::ResourcePtr<Patch>								mPatch;			///< Property: 'Patch'
		std::vector<nap::ResourcePtr<FixtureGroup>>			mGroups;		///< Property: 'Groups'
		/// Property: 'EndToEnd' — with 2+ groups: false (default) spreads each group across 0..1 on its
		/// own; true concatenates them into one run so the effect crosses the whole set once.
		bool												mEndToEnd = false;
	};
```

Add `#include "fixturegroup.h"` and register the two new properties in `trigger.cpp` (`"Groups"` Default, `"EndToEnd"` Default), removing `"Fixtures"`.

- [ ] **Step 2: Delete the unit filter**

`module/src/patchparameter.h`: delete `mUnits` and `appliesToUnit`. `module/src/patchparameter.cpp`: delete the `appliesToUnit` body and the `RTTI_PROPERTY("Units", ...)` line. Colour parameters now spread across every unit by construction (decision 2), so the filter has no remaining job.

- [ ] **Step 3: Rewrite fireTrigger over source lists**

Replace the fixture loop in `lxcontrolservice.cpp` (`:789-827`) with a per-spread-class source walk:

```cpp
			// Build each spread class's source list once. Per group (default) each group spans 0..1 on its
			// own; end-to-end concatenates them so the effect crosses the whole set once.
			for (lx::ESpreadClass cls : { lx::ESpreadClass::Fixture, lx::ESpreadClass::ColourUnit })
			{
				std::vector<std::vector<lx::Source>> runs;
				if (binding.mEndToEnd)
				{
					std::vector<lx::Source> all;
					for (auto& g : binding.mGroups)
						if (g != nullptr)
							for (auto& s : sourcesFor(*g, cls))
								all.emplace_back(s);
					runs.emplace_back(std::move(all));
				}
				else
				{
					for (auto& g : binding.mGroups)
						if (g != nullptr)
							runs.emplace_back(sourcesFor(*g, cls));
				}

				for (auto& run : runs)
				{
					const int count = static_cast<int>(run.size());
					for (int idx = 0; idx < count; ++idx)
					{
						lx::Source& src = run[idx];
						for (auto* channel : src.mFixture->getChannels())
						{
							if (lx::spreadClassOf(channel->getRole()) != cls)
								continue;
							// A ColourUnit source addresses exactly one unit; Fixture sources take unit 0.
							if (cls == lx::ESpreadClass::ColourUnit && channel->getUnitIndex() != src.mUnitIndex)
								continue;

							for (auto& param : patch->mParameters)
								for (int c = 0; c < param->getComponentCount(); ++c)
									if (param->getComponentRole(c) == channel->getRole())
										channel->pushClaim(activation.mId, patch, param.get(), c, idx, count, held);
						}
					}
				}
			}
```

Delete the now-unused `matched_count` / `ordered_fixtures` block above it.

- [ ] **Step 4: Replace the routing setters**

Rename `setRoutingFixtures` → `setRoutingGroups` (assigning `mGroups`), add `setRoutingSpread` (assigning `mEndToEnd`), and change `routeControl`'s last parameter to the group vector. Each still calls `markDirty()`. Update `ensureLifecycleTrigger`'s callers to default to `ensureDefaultGroup()` instead of the deleted `allFixtureIDs` lambda.

- [ ] **Step 5: Draw the routing group cell**

In `src/lxcontrolapp.cpp`, delete the `fixtureChips` lambda (`:1168-1189`) and `allFixtureIDs` (`:1276`). Give the patch combo a fixed width so the cell gets the remaining space, then per row:

```cpp
			// Bound groups as an ordered SET: chips + ">" separators, not a stack of pickers. Chip +
			// adjacent "x" is the same pattern the mod-matrix target list uses (see :934-942).
			auto& binding = trig->mBindings[0];
			int remove_g = -1;
			for (size_t g = 0; g < binding.mGroups.size(); ++g)
			{
				if (g > 0)
				{
					ImGui::SameLine(0.0f, 4.0f);
					ImGui::TextColored(lxtheme::muted(), ">");	// ASCII: fold() drops non-ASCII
				}
				ImGui::SameLine(0.0f, 4.0f);
				ImGui::PushID(static_cast<int>(g));
				lxtheme::Chip(binding.mGroups[g]->mName.c_str());
				ImGui::SameLine(0.0f, 2.0f);
				if (lxagent::SmallButton("x")) remove_g = static_cast<int>(g);
				ImGui::PopID();
			}
			if (remove_g >= 0)
			{
				auto next = binding.mGroups;
				next.erase(next.begin() + remove_g);
				mLxControlService->setRoutingGroups(*trig, next);
			}

			// "+ group": groups not already bound here, add-affordance at index 0 (same shape as the
			// mod-matrix "+ add" combo at :944-966).
			{
				std::vector<lx::FixtureGroup*> avail; std::vector<std::string> labels;
				for (auto& g : mLxControlService->getGroups())
				{
					bool bound = false;
					for (auto& b : binding.mGroups) if (b.get() == g.get()) { bound = true; break; }
					if (!bound) { avail.emplace_back(g.get()); labels.emplace_back(g->mName); }
				}
				if (!avail.empty())
				{
					std::vector<const char*> items; items.emplace_back("+ group");
					for (auto& s : labels) items.emplace_back(s.c_str());
					int sel = 0;
					ImGui::SameLine(); ImGui::SetNextItemWidth(104.0f);
					if (ImGui::Combo("##addgroup", &sel, items.data(), static_cast<int>(items.size())) && sel > 0)
					{
						auto next = binding.mGroups;
						next.emplace_back(avail[sel - 1]);
						mLxControlService->setRoutingGroups(*trig, next);
					}
				}
			}

			// Spread flag: only meaningful with 2+ groups, so only drawn then.
			if (binding.mGroups.size() > 1)
			{
				static const char* kSpread[] = { "per group", "end-to-end" };
				int s = binding.mEndToEnd ? 1 : 0;
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Text, lxtheme::mod2());	// violet: spread behaviour, not selection
				ImGui::SetNextItemWidth(112.0f);
				if (ImGui::Combo("##spread", &s, kSpread, 2))
					mLxControlService->setRoutingSpread(*trig, s == 1);
				ImGui::PopStyleColor();
			}
```

Apply the same cell to the Automatic (On load / On exit) rows, and change the Perform pad sublabels from a fixture/fx count to the bound group names (`"rgb_noise -> Front Three"`).

- [ ] **Step 6: BUILD**

KILL, BUILD. Expected: no errors, and:

```bash
grep -rn "mFixtureNames\|setRoutingFixtures\|appliesToUnit\|mUnits\|allFixtureIDs\|fixtureChips" module/src src
```

Expected: only `FixtureGroup::mFixtureNames` hits (the group's own member list) — no `PatchFixtureBinding` ones.

- [ ] **Step 7: Verify the 18-source spread — the headline behaviour**

```bash
rm -f data/user_content.json
```

Run RUN. Author: a patch with a **Colour** source + a **Chase** modulator on it; a control; route it to **All Fixtures**; load; fire (`click "Fire"`).

Expected:
- The chase steps across **18 colour units** — visible in RIG as the six unit swatches per fixture lighting in sequence, sweeping fixture 1 → 2 → 3. Before this task the same patch could only step 3 times. **This is the acceptance test for the whole refactor.**
- Add a second group to the same routing ⇒ a `per group ▾` combo appears; switching it to `end-to-end` makes one sweep cross both groups instead of two simultaneous sweeps.
- `lxui.ps1 shot` the PROGRAMS tab: group chips read as one ordered set, `x` per chip, no ragged wrapping.

- [ ] **Step 8: Commit**

```bash
git add module/src src
git commit -m "feat(engine): bindings hold FixtureGroups; fireTrigger spreads over source lists"
```

---

### Task 7: Modulatable Field inputs

**Files:**
- Modify: `module/src/modulator.h` (Field base class), `module/src/chasemodulator.{h,cpp}`, `module/src/noisemodulator.{h,cpp}` (inputs become parameters), `module/src/patch.{h,cpp}` (two-pass update), `module/src/lxcontrolservice.{h,cpp}` (create + persist input params), `src/lxtheme.h` (`InputRow`), `src/lxstyleguide.cpp`, `src/lxcontrolapp.cpp` (Field/Curve cards)

**Interfaces:**
- Consumes: Task 3's signature; Task 5's `Patch::blend`.
- Produces:
  - `class lx::FieldModulator : public lx::Modulator` — marker base for spatial/analytic modulators, with `virtual std::vector<lx::PatchParameter*> inputs() = 0;`
  - `bool lxtheme::InputRow(const char* label, float* v01, const char* drivenBy)`
  - `const char* lxcontrolApp::driverOf(const lx::Patch& patch, const lx::PatchParameter* input)`

- [ ] **Step 1: Add the Field base class**

In `module/src/modulator.h`:

```cpp
	/**
	 * A spatial modulator: computed analytically per frame from position + time, never from an authored
	 * napsequence curve. That is exactly why its inputs can be modulated — nothing has to be re-authored
	 * when one changes. Curve modulators (ADSR/AD/LFO/Step) bake their shape into a real sequence track,
	 * so their shape params stay authored (changing one calls generateCurve, which cannot run per frame).
	 */
	class NAPAPI FieldModulator : public Modulator
	{
		RTTI_ENABLE(Modulator)
	public:
		/** @return every modulatable input, in display order. Each is a PatchParameter so the existing
		 *  mod-matrix (Modulator::mTargets) can target it with no new target type. */
		virtual std::vector<PatchParameter*> inputs() = 0;
	};
```

Register with `RTTI_BEGIN_CLASS(lx::FieldModulator) RTTI_END_CLASS` in `modulator.cpp`. Change `ChaseModulator` and `NoiseModulator` to derive from `FieldModulator`.

- [ ] **Step 2: Convert Noise's inputs to parameters**

`module/src/noisemodulator.h` — replace `float mRate` / `float mSmoothing` with parameter pointers plus the ranges they map to:

```cpp
		std::vector<PatchParameter*> inputs() override		{ return { mRateInput.get(), mSmoothingInput.get() }; }
		float value(float pos01, int component) const override;

		nap::ResourcePtr<FloatParameter>	mRateInput;			///< Property: 'RateInput' 0..1 -> [0.05, 8] Hz
		nap::ResourcePtr<FloatParameter>	mSmoothingInput;	///< Property: 'SmoothingInput' 0..1 direct

	private:
		/** @return an input's authored-or-modulated 0..1 value mapped into [lo,hi]. Reads the parameter's
		 *  base; pass 1 of Patch::update has already written any modulated value into it. */
		float inputValue(const nap::ResourcePtr<FloatParameter>& in, float lo, float hi) const;
```

In `value()`, replace `mRate` with `inputValue(mRateInput, 0.05f, 8.0f)` and `mSmoothing` with `inputValue(mSmoothingInput, 0.0f, 1.0f)`. Do the same for Chase (`mRate` → `[0.05, 8]` Hz, `mPulseWidth` → `[0.01, 1]`).

- [ ] **Step 3: Create and persist the input parameters**

In `lxcontrolService::addModulator`, after constructing a Field modulator, create one `FloatParameter` per input (role `EChannelRole::Generic` — no rig channel maps Generic, so an input can never be claimed even if it leaks into a parameter list), `init()` it, assign it to the modulator, and record it in the `ModulatorEntry` so `save()` lists it as a root object. Add a `std::vector<rtti::ObjectPtr<lx::PatchParameter>> mInputs;` to `ModulatorEntry` and include it in `save()`'s root list.

- [ ] **Step 4: Make Patch::update two-pass**

`module/src/patch.cpp`:

```cpp
	void Patch::update(double deltaTime)
	{
		for (auto& modulator : mModulators)
			modulator->update(deltaTime);	// transport housekeeping

		// Pass 1: modulators whose targets are Field INPUTS write them now, so pass 2 (and every
		// evaluate() call this frame) sees the driven value. One pass per frame, non-recursive: a
		// user-authored A<->B pair resolves with one frame of latency instead of hanging, so no cycle
		// detection is needed at all.
		for (auto& modulator : mModulators)
		{
			for (auto& target : modulator->mTargets)
			{
				auto* input = target.get();
				if (input == nullptr || !isFieldInput(input))
					continue;
				if (auto* fp = rtti_cast<FloatParameter>(input))
					fp->mValue = nap::math::clamp(modulator->value(0.0f, 0), 0.0f, 1.0f);
			}
		}
	}
```

Add a private `bool isFieldInput(const PatchParameter* p) const` that scans this patch's `FieldModulator`s' `inputs()` for `p`. In `blend()`, **skip** modulators whose target is a Field input (they contribute to inputs, not to source values).

- [ ] **Step 5: Add InputRow**

In `src/lxtheme.h`:

```cpp
	/** One modulatable Field input: label, value, then EITHER a slider (authored) or a violet driven-by
	 *  marker when a Curve modulator owns it. Hiding the slider is correctness, not decoration: pass 1 of
	 *  Patch::update overwrites the value every frame, so an editable slider would fight it.
	 *  @return true when the slider changed. */
	inline bool InputRow(const char* label, float* v01, const char* drivenBy)
	{
		ImGui::AlignTextToFramePadding();
		ImGui::PushStyleColor(ImGuiCol_Text, drivenBy != nullptr ? mod2() : muted());
		ImGui::TextUnformatted(label);
		ImGui::PopStyleColor();
		ImGui::SameLine(96.0f);
		ImGui::Text("%.2f", *v01);
		ImGui::SameLine(148.0f);

		if (drivenBy != nullptr)
		{
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
		const bool changed = ImGui::SliderFloat((std::string("##in") + label).c_str(), v01, 0.0f, 1.0f, "");
		ImGui::PopStyleColor();
		return changed;
	}
```

Exercise it in `src/lxstyleguide.cpp` twice: once authored, once with `drivenBy = "AD"`.

- [ ] **Step 6: Draw the Field/Curve cards**

In `src/lxcontrolapp.cpp`'s modulator loop, add the family plate before the kind, and replace a Field modulator's raw `LabeledDrag`s with `InputRow`s:

```cpp
					const bool is_field = rtti_cast<lx::FieldModulator>(m.get()) != nullptr;
					lxtheme::Plate(is_field ? "Field" : "Curve", is_field ? lxtheme::mod() : lxtheme::mod2());
					ImGui::SameLine(); ImGui::TextColored(lxtheme::mod(), "%s", kind);
					...
					if (auto* fm = rtti_cast<lx::FieldModulator>(m.get()))
					{
						for (auto* in : fm->inputs())
						{
							if (auto* fp = rtti_cast<lx::FloatParameter>(in))
								if (lxtheme::InputRow(fp->mName.c_str(), &fp->mValue,
									driverOf(*patch.get(), in)))
									mLxControlService->markDirty();
						}
					}
```

with:

```cpp
	// ponytail: O(mods x targets) per input per frame -- both are single digits. Index it only if a patch
	// ever grows dozens of modulators.
	const char* lxcontrolApp::driverOf(const lx::Patch& patch, const lx::PatchParameter* input)
	{
		for (auto& m : patch.mModulators)
			for (auto& t : m->mTargets)
				if (t.get() == input) return m->mName.c_str();
		return nullptr;
	}
```

Extend `patchParamLabel` so a modulator's `-> target` summary can name a Field input as `"Noise . Rate"` (look the owner up via each `FieldModulator::inputs()`).

- [ ] **Step 7: BUILD, then verify journey 1 end to end**

KILL, BUILD, `rm -f data/user_content.json`, RUN. Author exactly the target journey: Colour source; **Noise** Field on Colour; **AD** Curve targeting Noise's **Rate** input (pick it from the mod-matrix `+ add` combo, which now lists Field inputs). Route to All Fixtures, load, fire.

Expected:
- Noise's `Rate` row shows **`driven by AD`** with a violet stub and **no slider**.
- On fire, the colour field visibly goes frantic and settles back over the AD's decay — the Field strip animates fast then slow.
- Smoothing still has a working slider (it is not driven).
- The `+ add` combo on the AD lists `Noise . Rate` as a target.

- [ ] **Step 8: Commit**

```bash
git add module/src src
git commit -m "feat(engine): Field modulators expose modulatable inputs; two-pass patch update"
```

---

### Task 8: Gradient Field modulator

**Files:**
- Create: `module/src/gradientmodulator.h`, `module/src/gradientmodulator.cpp`
- Modify: `src/lxcontrolapp.cpp` (add to the modulator type list)

**Interfaces:**
- Consumes: Task 7's `lx::FieldModulator`, `inputs()`, `inputValue`.
- Produces: `lx::GradientModulator` with `mStartInput`/`mEndInput` (`ResourcePtr<ColorParameter>`), `mPhaseInput`/`mPeriodInput` (`ResourcePtr<FloatParameter>`).

- [ ] **Step 1: Write the header**

```cpp
#pragma once

#include "modulator.h"

namespace lx
{
	/**
	 * A colour ramp across the source list: Start -> End, offset by Phase and repeated every Period of
	 * the position axis. Purely analytic (no sequence track), so every input is modulatable — an AD on
	 * Phase offsets the ramp like a chase; an LFO on Period breathes its scale.
	 * Endpoints are ColorParameters, not raw floats, so they can be driven too.
	 */
	class NAPAPI GradientModulator : public FieldModulator
	{
		RTTI_ENABLE(FieldModulator)
	public:
		std::vector<PatchParameter*> inputs() override;
		float value(float pos01, int component) const override;

		nap::ResourcePtr<ColorParameter>	mStartInput;	///< Property: 'StartInput'
		nap::ResourcePtr<ColorParameter>	mEndInput;		///< Property: 'EndInput'
		nap::ResourcePtr<FloatParameter>	mPhaseInput;	///< Property: 'PhaseInput' 0..1 of one period
		nap::ResourcePtr<FloatParameter>	mPeriodInput;	///< Property: 'PeriodInput' 0..1 -> [0.1, 4] spans
	};
}
```

- [ ] **Step 2: Write the implementation**

```cpp
	float GradientModulator::value(float pos01, int component) const
	{
		const float phase  = mPhaseInput  != nullptr ? mPhaseInput->mValue : 0.0f;
		const float period = 0.1f + 3.9f * (mPeriodInput != nullptr ? mPeriodInput->mValue : 0.25f);

		float p = std::fmod(pos01 / period + phase, 1.0f);
		if (p < 0.0f) p += 1.0f;
		const float tri = p < 0.5f ? p * 2.0f : (1.0f - p) * 2.0f;	// mirror, so a repeat never hard-cuts

		const int c = nap::math::clamp(component, 0, 2);
		const float s = mStartInput != nullptr ? mStartInput->getBaseValue(c) : 0.0f;
		const float e = mEndInput   != nullptr ? mEndInput->getBaseValue(c)   : 1.0f;
		return s + (e - s) * tri;
	}
```

`inputs()` returns `{ mStartInput.get(), mEndInput.get(), mPhaseInput.get(), mPeriodInput.get() }`. RTTI-register the class and its four properties. Extend `lxcontrolService::addModulator` to build a Gradient's four input parameters (two `ColorParameter`, two `FloatParameter`).

- [ ] **Step 3: Add it to the GUI type list**

`src/lxcontrolapp.cpp:895-898` — add `"Gradient"` to `mod_type_labels`, `RTTI_OF(lx::GradientModulator)` to `mod_types`, bump the `Combo` count and the `clamp` bound from 5 to 6. Add a `"GRADIENT"` case to the `kind` chain and include it in `is_voice_mod`'s Field test (or better: test `rtti_cast<lx::FieldModulator>`).

- [ ] **Step 4: REGEN, BUILD, verify journey 2**

Run REGEN (new `.cpp`), then KILL + BUILD, `rm -f data/user_content.json`, RUN. Author: Colour source; **Gradient** on Colour; an **AD** on its Phase; an **LFO** on its Period. Route to All Fixtures, fire.

Expected: a blue→gold ramp across the 18 colour units; firing offsets the ramp along the rig (chase-like) and the ramp's scale slowly grows and shrinks. The Field strip in the editor matches what the rig does.

- [ ] **Step 5: Commit**

```bash
git add module/src src
git commit -m "feat(engine): Gradient Field modulator with modulatable endpoints, phase and period"
```

---

### Task 9: Noise spatial coherence + Density

**Files:**
- Modify: `module/src/noisemodulator.h`/`.cpp`

**Interfaces:**
- Consumes: Task 7's `inputs()`/`inputValue`.
- Produces: `NoiseModulator::mDensityInput` (`ResourcePtr<FloatParameter>`, 0..1 → `[1, 24]` spatial cells), included in `inputs()`.

- [ ] **Step 1: Interpolate over position as well as time**

Today's hash is spatially *incoherent* by design (neighbouring sources are uncorrelated). Journey 1 asks for Perlin-ish transitions, so interpolate on both axes:

```cpp
	float NoiseModulator::value(float pos01, int component) const
	{
		const float rate    = inputValue(mRateInput, 0.05f, 8.0f);
		const float smooth  = inputValue(mSmoothingInput, 0.0f, 1.0f);
		const float density = inputValue(mDensityInput, 1.0f, 24.0f);

		const double t = mElapsed * static_cast<double>(std::max(rate, 0.001f));
		const double x = static_cast<double>(pos01) * density;

		const int ix = static_cast<int>(std::floor(x));
		const int it = static_cast<int>(std::floor(t));
		const float fx = smoothstep01(static_cast<float>(x - std::floor(x)));
		const float ft = smoothstep01(static_cast<float>(t - std::floor(t)));

		// Bilinear value noise over (space, time). Smoothing 0 collapses the time axis back to
		// sample-and-hold so the old hard-stepping look is still reachable.
		const float a = hash01(ix,     it,     component);
		const float b = hash01(ix + 1, it,     component);
		const float c = hash01(ix,     it + 1, component);
		const float d = hash01(ix + 1, it + 1, component);
		const float top = a + (b - a) * fx;
		const float bot = c + (d - c) * fx;
		return smooth <= 0.0f ? top : top + (bot - top) * ft * smooth;
	}
```

Add `mDensityInput` to the header, to `inputs()`, to the RTTI properties, and to `addModulator`'s input construction.

- [ ] **Step 2: BUILD and verify both looks**

KILL, BUILD, RUN. On a Colour source with one Noise:
- Density low (≈1) ⇒ the whole rig drifts together as one broad wash.
- Density high (≈24) ⇒ neighbouring colour units differ sharply, like the old per-voice look.
- Smoothing 0 ⇒ hard steps in time; Smoothing 1 ⇒ smooth crossfades.
- R/G/B differ from one another with **one** Noise modulator (this is what deleting `mSeed` bought).

- [ ] **Step 3: Commit**

```bash
git add module/src
git commit -m "feat(engine): Noise gains spatial coherence + Density input"
```

---

### Task 10 (optional): Field modulators shed the sequence graph

Defer freely — it removes weight, not a limitation. Value rises now that there are three Field types.

**Files:**
- Modify: `module/src/modulator.h`, `module/src/{chase,noise,gradient}modulator.{h,cpp}`, `module/src/lxcontrolservice.cpp` (`buildModulatorGraph`, `rewireModulator`, `save`, `loadUserContent`)

- [ ] **Step 1: Give FieldModulator its own transport**

Field modulators need only elapsed time and a playing flag — `NoiseModulator` already self-accumulates `mElapsed` *because* the player loops wrongly (`noisemodulator.h:40-44`). Move `mElapsed` + `mPlaying` onto `FieldModulator`, implement `onTrigger`/`onStop`/`update`/`isFinished` there, and delete each Field type's player use. Chase reads `mElapsed * rate` instead of `mPlayer->getPlayerTime()`.

- [ ] **Step 2: Stop building a graph for Field modulators**

In `buildModulatorGraph` / `rewireModulator`, skip clock/player/output/sink/editor construction when the modulator is a `FieldModulator`, and drop those (now-null) entries from `save()`'s root list. Delete both `generateCurve` dummy-curve overrides (`chasemodulator.cpp:25`, `noisemodulator.cpp:39`).

- [ ] **Step 3: BUILD and verify**

KILL, BUILD, `rm -f data/user_content.json`, RUN. Re-author a Chase and a Noise patch and fire it: behaviour must be unchanged. Then confirm the shrink:

```bash
grep -c "SequencePlayer\|SequenceEditor" data/user_content.json
```

Expected: a count reflecting only the Curve modulators present — zero if the patch has no ADSR/AD/LFO/Step.

- [ ] **Step 4: Commit**

```bash
git add module/src
git commit -m "refactor(engine): Field modulators drop the sequence graph and dummy curves"
```

---

### Task 11: Documentation

**Files:**
- Modify: `CLAUDE.md`, `docs/gui-refactor-findings.md`, `docs/single-multi-unification-plan.md`, `INITIAL-PROMPT.md`

- [ ] **Step 1: Update CLAUDE.md's current-model section**

Rewrite the vocabulary that this refactor invalidated:
- **Voice** is no longer "the per-fixture spread of a fired patch". Replace with: a **source** is a spread target (a fixture, or one colour unit of a fixture); a modulator is a function of normalised position over a group's ordered source list; `Modulator::valueForVoice` and `Patch::mTargetMode`/`mFixtureCount` no longer exist.
- Add **FixtureGroup** to the model list: ordered whole fixtures, live-authored in RIG, what a Control binds to.
- Add the **Field / Curve** split and the rule that a Field's inputs are modulatable while a Curve's shape params are not, with the `generateCurve` reason.
- Update the LTP paragraph: `FixtureChannelComponentInstance::resolveValue()` now calls `Patch::evaluate(param, index, count, component)`; there is no per-voice value buffer.
- Add a gotcha: `lxagent::fold()` drops non-ASCII, so button labels must be ASCII.

- [ ] **Step 2: Append to the design record**

In `docs/gui-refactor-findings.md`, record: the field strip (per-column sampling, and why coarser interpolation was rejected), the Field/Curve preview split (strip vs curve+playhead — same violet, told apart by form), the driven-input marker, and the rule **"counts live on the rig, never on the effect"**.

- [ ] **Step 3: Mark the plan done**

Add a short "shipped" note at the top of `docs/single-multi-unification-plan.md` pointing at this plan file, and note in `INITIAL-PROMPT.md` that the brief was implemented (or delete the file — it is a scratch brief, and the design doc supersedes it).

- [ ] **Step 4: Commit**

```bash
git add CLAUDE.md docs INITIAL-PROMPT.md
git commit -m "docs: record the single/multi effect unification model"
```

---

## Self-review

**Spec coverage** (against the eight decisions in `docs/single-multi-unification-plan.md`):

| Decision | Task |
|---|---|
| 1 Groups replace fixture selection | 1, 2, 6 |
| 2 Whole-fixture members; `mUnits` deleted | 1 (shape), 6 (deletion) |
| 3 Spread flag, per-group default | 6 |
| 4 Field / Curve families | 7 (base class + cards), 8 |
| 5 Field inputs modulatable, Curve shape not | 7 |
| 6 Gradient endpoints are ColorParameters | 8 |
| 7 `value(pos01, component)`; voice counts and seed gone | 3 |
| 8 No source count on the effect side | 5 (deletes the chips/meters), 2 (counts live on the group) |
| Engine G4 (activation owns values) | 5 — **superseded**: on-demand evaluation removes the buffer entirely |
| UI: field strip | 4, 5 |
| UI: RIG Groups editor | 2 |
| UI: routing group cell | 6 |
| Docs | 11 |

**Gaps found and closed while reviewing:**
- `cyclicPositions()` did not exist in the design doc — added in Task 3 with the chase-vs-gradient rationale, because one position convention cannot serve both.
- The design doc's per-activation value buffer is replaced by on-demand `evaluate()`. The plan is authoritative; Task 11 Step 3 notes the supersession.
- `text group` collided with `Control::mGroup` — fixed in Task 2 Step 6, including the skill doc.
- `Control::mGroup` (a Control's *device* label, serialized as `"Group"`, displayed as `"Device"`) is **not** renamed. It now reads confusingly next to `FixtureGroup`. Optional follow-up, deliberately out of scope: renaming it touches `control.{h,cpp}`, the service, the GUI and the persisted property name.

**Type consistency:** `evaluate(param, sourceIndex, sourceCount, component)` and `evaluateAt(param, pos01, component)` are used with those exact names and orders in Tasks 5 and 6; `pushClaim(activationId, patch, param, component, sourceIndex, sourceCount, held)` matches between Tasks 5 and 6; `sourcesFor(group, cls)` / `sourceCountsFor(group)` match between Tasks 2 and 6; `inputs()` matches between Tasks 7, 8, 9; `positionOf(m, index, count)` matches between Tasks 3, 5.

**Placeholder scan:** no TBD/TODO; every code step carries the actual code; every verification step names a concrete observable and what its failure looks like.
