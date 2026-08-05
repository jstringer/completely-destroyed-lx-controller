# Single / Multi effect unification — audit conclusions + implementation plan

> **Status: shipped** on `refactor/single-multi-fx-unification`. This file is the *design record* (audit,
> decisions, ImGui reasoning). The task-by-task execution plan, including what changed during
> implementation, is `docs/superpowers/plans/2026-08-05-single-multi-fx-unification.md`.
>
> **Superseded here:** §2's G4 described a per-activation value buffer. It isn't needed — a claim carries
> `(sourceIndex, sourceCount)`, so `FixtureChannelComponentInstance::resolveValue()` calls
> `Patch::evaluate` on demand and no buffer exists at all. §3.1's 48-segment `AddRectFilledMultiColor`
> strip was also replaced by per-column sampling (see the rewritten §3.1).

Branch: `refactor/single-multi-fx-unification`. Source brief: `INITIAL-PROMPT.md`.
UI reference: **Instrument UI Mockup v6** (`claude.ai/code/artifact/c4591199-0c7e-4af7-9cac-bd32167a67d6`),
drawn in the shipped language recorded in **Color as Structure**
(`claude.ai/code/artifact/66ed37d2-6902-4224-a757-4b2b6b7ee898`).

---

## 1. What we decided

| # | Decision |
|---|---|
| 1 | **Groups replace fixture selection.** A routing binds Control → Patch → one or more `FixtureGroup`s. The per-routing `S1/S2/S3` chip set is deleted. |
| 2 | **Group members are whole fixtures.** No sub-unit subsets — a fixture's six colour units are defined at the fixture level and abstracted away by the time it joins a group. `PatchParameter::mUnits` is therefore deleted. |
| 3 | **Multi-group spread is one flag** on the binding: per-group (each group spans 0..1, default) or end-to-end (concatenated as one run). Not a policy system. |
| 4 | **Two modulator families.** **Field** = spatial, analytic, drives sources (Chase / Noise / Gradient). **Curve** = temporal, curve-authored, drives a Field's inputs *or* a source directly (ADSR / AD / LFO / Step). |
| 5 | **A Field's inputs are modulatable; a Curve's shape params are not.** Not a taste call — Curve params bake into a `napsequence` track via `generateCurve`→`authorFloatCurve`, which cannot run per frame. Every input in both target UX journeys is analytic, so nothing is lost. |
| 6 | **Gradient endpoints are `ColorParameter` inputs**, so Start/End are modulatable too. |
| 7 | **One evaluation signature:** `float value(float pos01, int component) const`. Replaces `value()` + `valueForVoice(int)`. Kills `setVoiceCount`, both `mVoiceCount` caches, and `NoiseModulator::mSeed`. |
| 8 | **The effect side never states a source count.** No count chips, no voice list, no fixture names in the patch editor. Counts appear only where they are a real fact: on a group, and on the rig. |

---

## 2. Engine phases

Order is dependency order. `regenerate.bat` is required for **G1** and **G6** (new `.cpp` files) — a new
source file left out of the CMake file list builds "successfully" while never compiling.

| Phase | Files | Work |
|---|---|---|
| **G1** Group resource | `module/src/fixturegroup.{h,cpp}` (new); `lxcontrolservice` CRUD + `save()` + `loadUserContent` | `nap::Resource` (data, like `Control`/`MidiBinding` — not an Entity): `mName`, `std::vector<std::string> mFixtureNames` (ordered, whole fixtures). Must be listed explicitly in `save()`'s root-object set — `serializeObjects` does not pull in `Default`-pointer targets. |
| **G2** Source enumeration | `lxcontrolservice` | `sourcesFor(group, roleClass) -> std::vector<Source>` where `Source = {FixtureComponentInstance*, unitIndex}`. Lifts the inline role/unit match out of `fireTrigger` (`lxcontrolservice.cpp:814-825`). Delete `PatchParameter::mUnits` + `appliesToUnit()`. |
| **G3** Position API | `modulator.h`; all six modulators | `value(pos01, component)`. `ChaseModulator::valueForVoice` already *is* position maths (`chasemodulator.cpp:61`) — `wrapFrac(t - pos01)`. Noise decorrelates on `component` instead of a per-instance seed. |
| **G4** Activation owns live values | `patch.{h,cpp}`, `patchparameter.{h,cpp}`, `lxcontrolservice` | `mCurrentValues` leaves `PatchParameter` entirely (authored base only). Delete `mTargetMode`, `mFixtureCount`, `syncPatchFixtureCount`. Fixes the shared-buffer collision two activations of one patch have today. |
| **G5** Modulatable inputs | `modulator.h`, Field modulators, `Patch::update` | Field inputs become `ResourcePtr<FloatParameter>` / `ColorParameter`, read mapped to the modulator's own range. `mTargets` needs **no** new target type. Two passes per frame (inputs, then sources) — single-pass, non-recursive, so no cycle detection is ever needed. Field modulators also shed their `SequencePlayer` graph and dummy curves (`chasemodulator.cpp:25`, `noisemodulator.cpp:39`). |
| **G6** Gradient + Noise | `module/src/gradientmodulator.{h,cpp}` (new); `noisemodulator` | Gradient: Start/End colour, Phase, Period. Noise gains spatial coherence + Density — today's `hash01(voice, step, seed)` is deliberately spatially *incoherent*, which is a different look from the Perlin-ish field asked for. |
| **G7** Binding → groups | `trigger.h`, `lxcontrolservice` routing helpers | `PatchFixtureBinding::mFixtureNames` → `std::vector<ResourcePtr<FixtureGroup>> mGroups` + `bool mEndToEnd`. `setRoutingFixtures` → `setRoutingGroups`. |
| **G8** GUI | `src/lxcontrolapp.{h,cpp}`, `src/lxtheme.h`, `src/lxstyleguide.cpp` | Section 3 below. |

**Migration:** existing `data/user_content.json` carries `TargetMode` / `FixtureCount` / `mFixtureNames`.
Per the standing "user_content is placeholder" directive: **delete the file, write no migration code.**

**Remaining ceiling (unchanged by this work):** `mPlayer` / `mElapsed` still live on the Modulator, so two
simultaneous activations of one patch share a playhead. Fork still covers that; per-activation modulator
instances would mean cloning the sequence graph per fire.

---

## 3. UI changes and how to build them in ImGui

Everything below is ImGui **1.76** (`system_modules/napimgui/include/imgui/imgui.h:62`), no `imgui_internal`,
drawn from `lxcontrolApp::update()` after `mGuiService->selectWindow(mRenderWindow)` — never from `render()`.

### 3.0 Constraints that shape the widgets

1. **Slabs are single-level.** `lxtheme::SlabBegin` uses one `ImDrawList::ChannelsSplit(2)`; nesting a slab
   inside a slab corrupts the splitter. The mockup already honours this — a `Plate` sits **above** a run of
   *sibling* slabs, never inside a wrapping one. Modulator cards are siblings with a violet spine, alternating
   `slab()` / `slab2()` (as `mod_index & 1` already does at `lxcontrolapp.cpp:922`).
2. **Buttons must be ASCII and must go through `lxagent`.** `lxagent::fold()` (`lxagent.h:69-91`) drops every
   byte `>= 0x80`, so a button labelled `✕`, `⇄` or `›` folds to an **empty key** — unaddressable, and every
   such button collides with the others. Use `x`, `Rev`, `>`. Non-interactive *text* may still use Latin-1
   glyphs (`«` `»`, as the Live Bar cue arrows already do), but nothing above U+00FF: the bundled Cascadia Code
   atlas is built with ImGui's default glyph ranges, so `›` (U+203A) and `✕` (U+2715) render as a fallback box.
3. **Duplicate labels are fine.** `consumeClick` suffixes repeats per frame (`x`, `x#2`, `x#3`) and the ack's
   `CLICKABLE:` line lists the exact keys. Still `PushID` per row for ImGui's own identity.
4. **The bridge cannot drive combos.** Only `Button`/`SmallButton`/tabs are addressable. Group *binding* and
   the spread flag are combos, so `lx-drive` can screenshot them but not operate them — same limitation the
   Learn workflow already has. Where a headless check matters, keep the action on a button.
5. **Combo item lifetime.** `ImGui::Combo(label, &idx, const char* const items[], count)` does not copy — build
   a `std::vector<std::string>` *and* a `std::vector<const char*>` into it in the same scope, exactly as the
   patch/control combos already do (`lxcontrolapp.cpp:1207-1216`).
6. **Form state is keyed by `mID`, not pointer** (`lxcontrolapp.h:99`): service mutations call `save()`, and a
   rewritten object graph can leave a raw pointer dangling. New group form state follows that rule.

### 3.1 New helper — `lxtheme::FieldStrip` (the one new widget)

The field readout replaces every per-voice meter row. Note the app needs **only the continuous strip**: the
mockup's sampled band is an explainer device, and the rig-side truth is already the RIG tab's per-fixture
faders + six unit swatches.

```cpp
/** Horizontal field strip: a modulator (or a patch parameter's resolved field) sampled as a continuous
 *  function of position 0..1 -- the readout that replaces the per-voice meter row. `components` == 3 ->
 *  RGB; == 1 -> the gold luminance ramp (a float field must not invent a new hue).
 *
 *  One sample per PIXEL COLUMN, deliberately: the fields that matter most are hard-edged (Chase's pulse,
 *  Noise at Smoothing 0), and any coarser sampling + interpolation smears those edges or misses them
 *  outright -- a Chase at PulseWidth 0.05 can vanish between samples. Per-column is also the simplest
 *  thing that can work: no segment count to tune, no interpolation mode to choose, exact to display
 *  resolution. It matches the approved mockup's canvas, which samples the same way. */
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
			col = ImGui::ColorConvertFloat4ToU32(ImVec4(field(pos, 0), field(pos, 1), field(pos, 2), 1.0f));
		}
		else
		{
			const float v = nap::math::clamp(field(pos, 0), 0.0f, 1.0f);
			const ImVec4 g = live();
			col = ImGui::ColorConvertFloat4ToU32(ImVec4(g.x * v, g.y * v, g.z * v, 1.0f));
		}
		const float x0 = p.x + size.x * i / cols;
		const float x1 = p.x + size.x * (i + 1) / cols;
		dl->AddRectFilled(ImVec2(x0, p.y), ImVec2(x1 + 0.5f, p.y + size.y), col);	// .5 overlap: no seams
	}
	ImGui::Dummy(size);
}
```

- **Cost:** a ~360px strip is 360 quads / ~1440 verts; a handful of strips is a few thousand verts per frame,
  against the tens of thousands ImGui pushes routinely. *ponytail: if this ever profiles hot the upgrade is
  `PrimReserve` + direct vertex writes (what a gradient library does internally), NOT a coarser sample count --
  coarser sampling buys frames by drawing something the rig isn't doing.*
- **Idle behaviour is honest:** when nothing is firing, Field modulators aren't playing, so the strip shows the
  static shape at t=0. No stale live values, because there are none to read.
- **Display bound, not a lie:** 300 sources on a 360px strip can't be individually distinguishable. Adjacent
  sources genuinely overlap in space, so the readout degrades to *the field* — which is the intent (decision 8).
- Add it to `src/lxstyleguide.cpp` — every `lxtheme` helper is exercised in the Style Guide test bed.

**Rejected: [imgui_gradient](https://github.com/Coollab-Art/imgui_gradient).** `ImGG::Gradient` is a **mark
list** (positions + colours + interpolation/wrap mode, sampled by `gradient().at(pos)`) and `widget()` is an
interactive *editor* — draggable marks, colour popups, drag-down-to-delete. Rendering `f(pos, component)`
through it means rebuilding and sorting marks every frame, and hard-edged fields need a mark *pair* per edge
(Chase), per step (sample-and-hold Noise), or per repeat (Gradient's period). On top of that it is a CMake
dependency compiled against NAP's vendored ImGui **1.76 (2020)**, and 1.82 alone renamed
`ImDrawCornerFlags`→`ImDrawFlags`; breakage means forking it.

**Where it would earn its place: G6, authoring.** If Gradient's two endpoints prove too few, a multi-stop
colour ramp *is* a mark editor — `ImGG::Gradient` becomes the modulator's stored ramp and `at(pos)` its
evaluator. Test the 1.76 compile first with a throwaway `add_subdirectory` in `app_extra.cmake`.

### 3.2 New helper — `lxtheme::InputRow` (a modulatable Field input)

```cpp
/** One modulatable Field input: uppercase label, value, then EITHER a slider (authored) or a violet
 *  driven-by marker when a Curve modulator owns it. Hiding the slider is correctness, not decoration:
 *  pass 1 of Patch::update overwrites the value every frame, so an editable slider would fight it.
 *  Returns true when the slider changed. */
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
		// 4px violet stub + source name: the same visual grammar as the spine, at row scale.
		const ImVec2 p = ImGui::GetCursorScreenPos();
		const float  h = ImGui::GetTextLineHeight();
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

Finding the driver is a scan of the patch's modulators for one whose `mTargets` contains this input param:

```cpp
// ponytail: O(mods x targets) per input per frame -- both are single digits. Index it only if a patch
// ever grows dozens of modulators.
const char* driverOf(const lx::Patch& patch, const lx::PatchParameter* input)
{
	for (auto& m : patch.mModulators)
		for (auto& t : m->mTargets)
			if (t.get() == input) return m->mName.c_str();
	return nullptr;
}
```

### 3.3 PROGRAMS — routing rows

Replace the `fixtureChips` lambda (`lxcontrolapp.cpp:1168-1189`) with a group cell. The bound groups read as
an **ordered set** (chips + `>` separators), not a stack of pickers:

```cpp
// Bound groups, in order. Chip + adjacent "x" is the same pattern the mod-matrix target list already
// uses (lxcontrolapp.cpp:934-942) -- reuse it rather than inventing a removable-chip widget.
auto& binding  = trig->mBindings[0];
int   remove_g = -1;
for (size_t gi = 0; gi < binding.mGroups.size(); ++gi)
{
	if (gi > 0)
	{
		// ASCII ">" on purpose: lxagent::fold drops non-ASCII, and U+203A is outside the font atlas.
		ImGui::SameLine(0.0f, 4.0f);
		ImGui::TextColored(lxtheme::muted(), ">");
	}
	ImGui::SameLine(0.0f, 4.0f);
	ImGui::PushID(static_cast<int>(gi));
	lxtheme::Chip(binding.mGroups[gi]->mName.c_str());
	ImGui::SameLine(0.0f, 2.0f);
	if (lxagent::SmallButton("x"))			// addressable as x, x#2, x#3 ...
		remove_g = static_cast<int>(gi);
	ImGui::PopID();
}
if (remove_g >= 0)
{
	auto next = binding.mGroups;
	next.erase(next.begin() + remove_g);
	mLxControlService->setRoutingGroups(*trig, next);
}

// "+ group": groups not yet bound here, as a combo with the add affordance at index 0 -- the same shape
// as the mod-matrix "+ add" combo (lxcontrolapp.cpp:944-966).
// ... build avail/labels, then Combo("##addgroup", &sel, items, n) && sel > 0 -> setRoutingGroups(...)

// Spread flag: only meaningful with 2+ groups, so only drawn then.
if (binding.mGroups.size() > 1)
{
	static const char* spread_labels[] = { "per group", "end-to-end" };
	int s = binding.mEndToEnd ? 1 : 0;
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Text, lxtheme::mod2());	// violet: this is spread behaviour, not a selection
	ImGui::SetNextItemWidth(112.0f);
	if (ImGui::Combo("##spread", &s, spread_labels, 2))
		mLxControlService->setRoutingSpread(*trig, s == 1);
	ImGui::PopStyleColor();
}
```

- The patch combo gets a **fixed** width (~138px, `flex:none` in the mockup) so the group cell takes the
  remaining space; today it is `flex:1` and would squeeze the chips.
- Column offsets in these rows are hard-coded `SameLine(x)` positions (`lxcontrolapp.cpp:1215-1249`) to make
  the rows read as a table. Re-derive them once after the cell contents change — the group cell is variable
  width, so anything after it must stay right-aligned via `GetContentRegionAvail()`, not a fixed offset.
- **Deleted from this panel:** the derived source-count text. Decision 8.
- The Automatic (On load / On exit) rows use the identical cell; `allFixtureIDs` (`lxcontrolapp.cpp:1276`)
  becomes "default to the All Fixtures group".

### 3.4 RIG — the Groups editor

RIG is currently read-only; this makes part of it editable. Art-Net readout and the fixture strips are
unchanged (bordered monitors, gold spine only while emitting — they are not collections, so no slab).

```
Plate("Groups", accent())          // teal: groups are what you select/bind
  per group:  SlabBegin(gi & 1 ? slab2() : slab(), accent())
                "::" drag affordance (text only)  +  name
                member chips, each: Chip(fixtureName) + SmallButton("x")
                SmallButton("^") / SmallButton("v")   // reorder; no DnD
                SmallButton("Rev")                    // reverse the whole run
                derived-count chips, right-aligned:  "3 dimmer" "3 strobe" "18 colour"
              SlabEnd()
  InputText("##newgroup") + Button("+ New group")
```

- **Reordering is `^` / `v` buttons, not drag-and-drop.** 1.76 has `BeginDragDropSource`/`Target`, but it is
  more code, and the agent bridge cannot drive a drag at all. `Rev` covers the common "spread the other way"
  case in one click. *ponytail: arrows + Rev; add DnD only if reordering a long group actually annoys.*
- Order **is** semantics here (it sets the spread direction), so the member chips show their index — this is
  one of the few places a numbered marker encodes something true.
- Derived count chips are the **only** place in the app that states a source count, alongside the rig's own
  per-fixture readout. They come from `sourcesFor()` (G2); nothing is typed or stored.
- New app state: `char mNewGroupName[64]` plus, if a selected-group notion is wanted, a `std::string`
  **mID** — not a pointer (§3.0.6).

### 3.5 PROGRAMS — patch editor

**Current** — one `FieldStrip` per source parameter, no counts, no voice tooltips:

```cpp
lxtheme::Plate("Current", lxtheme::muted());
lxtheme::SlabBegin(lxtheme::slab(), lxtheme::muted());
for (auto& p : patch->mParameters)
{
	ImGui::PushID(p.get());
	ImGui::AlignTextToFramePadding();
	ImGui::PushStyleColor(ImGuiCol_Text, lxtheme::muted());
	ImGui::TextUnformatted(patchParamLabel(p.get()).c_str());
	ImGui::PopStyleColor();

	// Evaluate the patch's field at the strip's own sample positions -- the GUI no longer reads a
	// per-voice buffer (there isn't one after G4), it CALLS the evaluator. Works while idle, and can
	// never show a stale value.
	const int comps = p->getComponentCount();
	lxtheme::FieldStrip([&](float pos, int c) { return patch->evaluate(*p, pos, c); }, comps,
		ImVec2(-1.0f, 30.0f));
	ImGui::PopID();
}
lxtheme::SlabEnd();
```

**Modulation** — the `Plate` sits above sibling per-modulator slabs (never nested, §3.0.1). Card header gains
a family tag before the kind:

```cpp
// Family tag: violet fill for Field, light violet for Curve. Both stay violet -- the family is told
// apart by its PREVIEW (strip vs curve+playhead), not by a fourth hue.
const bool is_field = rtti_cast<lx::FieldModulator>(m.get()) != nullptr;
lxtheme::Plate(is_field ? "Field" : "Curve", is_field ? lxtheme::mod() : lxtheme::mod2());
ImGui::SameLine(); ImGui::TextColored(lxtheme::mod(), "%s", kind);
ImGui::SameLine(); ImGui::TextColored(lxtheme::mod2(), "-> %s", targetSummary(m.get()).c_str());
```

- **Field card:** `FieldStrip` preview, then one `InputRow` per input. A `ColorParameter` input (Gradient
  Start/End) uses `ImGui::ColorEdit3(..., ImGuiColorEditFlags_NoInputs)` in place of the slider, and the same
  driven-by branch when a Curve owns it.
- **Curve card:** unchanged — `lxtheme::PlayheadPreview(shape, 128, m->playheadPhase(), ...)` fed by
  `sampleShape`. Shape params keep their `regen()` on change (decision 5).
- The `-> target` summary must now name either a source parameter *or* another modulator's input
  (`"Noise . Rate"`). Extend `patchParamLabel` with an owner lookup rather than adding a second labeller.
- **Deleted:** the Spread combo + voice chip (`lxcontrolapp.cpp:780-791`), the per-voice `Current`
  meter/swatch loops (`:801-848`), the `is_voice_mod` per-voice `ProgressBar` row (`:968-980`), and
  `describePatchVoice` entirely (`lxcontrolapp.h:78`, `lxcontrolapp.cpp:674-695`).

### 3.6 PERFORM

Pad sublabels change from a fixture/fx count to the bound groups: `rgb_noise -> Front Three`. Pad labels are
the agent's click target and may stay two-line — `fold()` collapses embedded whitespace (`lxagent.h:69`), which
is exactly why the existing two-line pads are addressable.

### 3.7 Verification

`lx-drive` can reach: tab switches, `+ New group`, `x`, `^`, `v`, `Rev`, `Fire`/`Stop`, `+ route a control`,
Perform pads. It **cannot** operate the group picker or spread combo (§3.0.4) — check those by screenshot.
Judge the captures aesthetically, not just for rule-compliance: at 18 and 300 sources the whole point is that
the strip still *reads*.

---

## 4. Docs to update when this lands

- `CLAUDE.md` — the "Current model" section: **Voice** is no longer "the per-fixture spread of a fired patch"
  but a normalised position over a group's source list; add `FixtureGroup`; drop `Patch::mTargetMode` /
  `mFixtureCount`; note Field vs Curve.
- `docs/gui-refactor-findings.md` — append the field strip and the Field/Curve preview split to the design
  record, with the "counts live on the rig, never on the effect" rule.
- `.claude/skills/lx-drive/SKILL.md` — no new verbs, but note the group picker as another combo the bridge
  can't drive.
