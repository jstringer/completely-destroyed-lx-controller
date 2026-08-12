# lxcontrol GUI/UX Refactor — Audit & Expert Findings

Status: **findings compiled, awaiting direction to iterate.** Produced from a verified current-state architecture map (nap-explore) + three expert reviews (UI/visual, UX/HCI, UX-engineer), each grounded in the code, the reference screenshots in `docs/gui-refactor-ref-screenshots/`, the `nap-gui` skill constraints, and the "synthesizer for light" ethos. All three were given an explicit greenfield mandate (remake if warranted).

---

## 0. The one big idea (all three, independently)

**The engine is already a clean mod-matrix synthesizer. The GUI's whole problem is that it draws the C++ class graph 1:1 instead of the three things a player actually thinks in.**

The data model (`Controller` = a control + mode + MIDI-learn; `Effect` = a patch of parameters + modulators; `Trigger` = play-patches-on-fixtures; `ControllerMapping` = per-program control→trigger; `Program` = loadable scene; LTP output arbitration) is *good* and maps cleanly onto synth concepts. Almost every pain point is **presentation and naming**, plus **one mechanism** (the save→hot-reload loop) that causes the runtime bugs. You largely don't need to rebuild the engine — you need to stop drawing it, rename it, and fix the persistence loop.

---

## 1. Consensus — what all three agree on

1. **Rename off the audio/synth vocabulary.** "Effect" → **Patch** is unanimous (it's a voice/patch; "Effect" collides with the audio-FX sense). "Controller" (the single-control class) is a misnomer → **Control** (a Pad/Knob); the word "Controller" is triple-overloaded (virtual control / Art-Net device / sequence controller) and must be split.
2. **Flatten the Programs tree into a matrix.** The 4–5 level `Program › Controller Mappings › control › Bindings › row` tree (screenshots `programs_00/02/07`) is really a flat **control → trigger → patch → fixtures** table. Draw it as one row per mapping (`Columns()`/`BeginChild`, not `TreeNode`). This also designs out the dropdown-overdraw bug visible in `programs_07`.
3. **Kill the ordering traps.** "Create a Controller in the MIDI tab first" appears 5× — the app apologizing for its own IA. Replace with inline "+ new cue" / "learn a pad in place."
4. **Give Art-Net/DMX config a home.** The Fixtures tab is a wall of read-only `0.000` sliders; there is no universe/IP/patch UI anywhere. Add a **RIG** page.
5. **The instrument must *show state*.** Live output, active program, modulation, and armed/learning should read as **color + motion**, not grey prose. Fix the limp grey "(none loaded – output is dark)".
6. **Theme via data, not literals.** Add one `IMGuiServiceConfiguration` block + a semantic palette (`getPalette()`), replacing every hard-coded `ImVec4`.
7. **Keep modulators as knob-edited shapes with a live preview** (do NOT introduce a `SequenceEditorGUI` timeline — wrong metaphor, and it re-adds the very complexity we're removing). Upgrade the `PlotLines` preview with a moving playhead.
8. **ImGui 1.76 constraints are real**: no Tables, no docking, no `BeginDisabled`. Everything above is built with `Columns()`/`BeginChild`/`Selectable`/`VSliderFloat`/`PlotLines`/draw-list, per-item `PushID(mID)`.

---

## 2. The decisions to make (where the experts diverge) — with my recommendation

### Decision A — How bold on naming?
- **Conservative (UI designer):** rename **tab labels only** (Fixtures→RIG, Effects→PATCHES, Programs→PROGRAMS, MIDI→CONTROL); leave all C++ classes as-is.
- **Moderate (UX-engineer):** rename the **model classes** where it kills ambiguity — Effect→Patch, EffectParameter→PatchParameter, Controller→Control — but **keep** `Program`, `Trigger`, `Modulator` (argues a synth "program" and "trigger" are already correct). Collapse the 3 empty Trigger subtypes to one enum. Do it with a one-time type-string migration map at the loader (else `user_content.json` fails to load and silently resets).
- **Bold (HCI designer):** full lighting-desk lexicon — Effect→Patch, **Program→Scene**, **Trigger→Cue** (+ On-Load/Exit for lifecycle, Fire/Release/Test for the verb), Controller→**Pad/Knob**, Slot→**Voice** (per-note polyphony), Mode→**Hold/Latch/Trig**, Load→**Recall**.

**My recommendation:** **Moderate at the class level, selectively bold at the UI level.** Effect→Patch and Controller→Control are unambiguously right — do them as real renames (with the migration map). For the genuinely contested ones (Program→Scene, Trigger→Cue): keep the *class* names stable but let the *UI words* be the richer desk terms if you like them — UI labels are free to change, class renames cost a migration. The one I'd actually flip in the model too is **Slot→Voice** — "per-note polyphony" is the core synth idea and deserves the real name. (Program→Scene is a coin-flip: "Scene" avoids the MIDI Program-Change collision and is the operator's word; "Program" is defensible. Your call — see question.)

### Decision B — How far to restructure?
- **Minimum (unanimous):** flatten Programs→matrix, add RIG page, restyle.
- **Bolder (HCI + partially UI):** add a persistent **Live Bar** + a **Perform/Edit mode toggle** — performing becomes an always-on view, not a tab you re-expand. This is the biggest "it's an instrument now" payoff and is cheap in immediate mode (one bool gating `BeginTabBar`).

**My recommendation:** do the minimum **and** the Perform/Edit split. The perform view is read+fire over the *existing* service API — near-zero model cost, maximal UX gain.

### Decision C — Commit to the persistence rewrite (Phase A) first?
The UX-engineer's headline: **the hot-reload of authored content serves no purpose and causes every runtime bug you have** (the mid-animation reset; direct edits silently lost). The service already holds and mutates live pointers; the ResourceManager then destroys/recreates them behind its back and heroically re-wires. Owning the load (`readJSONFileObjects` + `DefaultLinkResolver`) + a debounced dirty-save **deletes code**, kills both bugs, and makes rename/theming/direct-edit-persistence trivial downstream. It's the highest-leverage change — and also the highest-risk (you own link resolution).

**My recommendation (and the ponytail-correct move):** yes, do Phase A first. The "lazy" solution here is the rewrite, because it removes far more than it adds. It's drastic but it's *deletion*.

### Decision D — Runtime fixture re-patching?
Adding/removing fixtures live is genuinely expensive (fixtures are Scene Entities; NAP can't cheaply add Entities to a running Scene — the same limit that pushed effects/triggers to be data). **Recommendation:** keep rig *topology* in `objects.json`/Napkin (a re-cabling event is restart-acceptable), surface it **read-only** in the RIG page, and make only the cheap knobs live (StartChannel, DisplayName, Art-Net **Frequency** — which is a real hardware calibration knob). Defer true live re-patching unless the redesign demands it.

---

## 3. Recommended unified plan (maps the three lenses onto shippable phases)

Each phase is independently shippable; ordering is deliberate.

- **Phase A — Own the persistence loop.** Manual deserialize + `DefaultLinkResolver`; debounced dirty-flush `save()`; delete the reload/rewire path; persist active-program mID. *Fixes the mid-animation reset + direct-edit loss; net-negative LOC.* **Highest risk** (link resolution). No visible UI change.
- **Phase B — GUI split + theming scaffold.** Break the 950-line `update()` into per-screen view structs + a `UiContext`; add the `IMGuiServiceConfiguration` + `lxtheme.h` semantic palette + ~5 widget helpers; delete the 3 dead `ParameterGUI` resources. Pure restructure — prerequisite for everything the designers drew.
- **Phase C — Model rename + Trigger-subtype collapse.** Effect→Patch, Controller→Control, Slot→Voice (per Decision A), Trigger subtypes→enum `Kind`, behind a load-time migration map. **High risk to existing authored content** — unit-test the migration on a real captured `user_content.json`; keep the `.bak` guard; rename in one pass.
- **Phase D — The instrument UX.** Flatten Programs→matrix; RIG page (read-only patch + Art-Net + live cheap knobs); **Perform/Edit mode + Live Bar**; the four-hue semantic state system (live-output dot, armed=breathing-cyan, modulation=violet-with-motion); swatch color channels + filling faders + live playhead previews.
- **Phase E — OSC as a second input.** Add `OscBinding` beside `MidiBinding` + `onOscEvent` reusing the identical Control→Trigger path (the model is *already* input-agnostic — `Control` holds no MIDI). Add `naposc`, an OSC receiver in `objects.json`, extend Learn. Cheap.
- **Phase F (optional, Large) — Fixtures as data.** Only if hot-repatching becomes required. Convert fixtures from Scene Entities to `FixturePatch` resources iterated by the service. Fallback: edit-JSON-and-restart.

---

## 4. Side note — `CLAUDE.md` is significantly stale

The repo's `CLAUDE.md` documents a previous generation (tabs "Presets"; classes `Preset`/`MidiMapping`/`EffectLayer`/`FixtureChannelBinding`; per-effect `SequenceEditorGUI` timelines) that **no longer exist**. Current model is `Program`/`Trigger`/`Controller`/`Effect`/`Modulator` with an LTP claim stack and headless-curve modulators. `data/presets/*.json` are dead leftovers. This should be refreshed as part of the effort.

---

# Appendix A — UX / HCI + Information Architecture report (full)

_(see below — verbatim expert output)_

# Appendix B — UI / Visual System report (full)

_(see below — verbatim expert output)_

# Appendix C — UX-Engineer Architecture & Refactor report (full)

_(see below — verbatim expert output)_

> The three full appendix reports are reproduced in the chat thread that generated this document; paste them here if a standalone archive is wanted. This file is the synthesis + decision record.

---

# 5. Locked design decisions (running record)

Decided with the owner during the mockup iterations (artifact: the `lxcontrol` UI mockup, currently v5):

- **Lexicon (full desk/synth vocabulary):** **Patch** = a light-voice (color/dimmer source + modulators). **Program** = the loadable unit *and* the workspace tab — a collection of control→patch routings + on-load/on-exit behavior; one loaded at a time. (This *reverted* the earlier Program→Scene idea — "Program" is the synth term the owner wants, and it fits binding a program-load to a MIDI Program-Change.) **Voice** = the live, evolving result of a Control firing a Patch (per-fixture spread = one Voice per fixture). **Control** = one pad/knob, with a behavior (**Hold/Latch/Trig** = Momentary/Toggle/FireOnly) + its external MIDI/OSC binding. **Controller** = the virtual *device* that groups Controls — the ONLY place external bindings are configured. **Cue is retired.**
- **Structure:** 3 tabs — **RIG · PROGRAMS · CONTROLS** — plus a persistent **Live Bar** with a **Perform/Edit** toggle. PROGRAMS is a **3-panel workspace** (Programs list | routing rows + Automatic + Output-mix | in-context shared Patch editor). Patches are shared across Programs; edited in context; "Fork" to make a copy.
- **Separation of concerns (owner correction):** external device bindings live *only* in CONTROLS; the PROGRAMS routing rows only pick an *existing* Control from a dropdown.
- **Visual direction:** committed to **"terminal / luminous"** — mono-brutalist language (monospace, hairlines, square corners, `[ bracketed ]` headers) + a luminous teal/cyan palette; **gold = live output, violet = modulation, teal/cyan = interaction/armed, rose = destructive**. Single dark instrument theme by choice.
- **Spacing/boundary policy:** borders only on the chassis + true containers + leaf objects; zones separated by full-bleed rule + gap; list rows are borderless (hairline divider + accent-on-selected). Scale: region 24 > zone 20 > section 12 > item 6; frame padding small and fixed.
- **Persistence:** commit to **Phase A first** (own the load, debounced dirty-save, kill the hot-reload) — it deletes code and removes the live-editing bugs.
- **Arbitration/Voice:** commit to **held-priority LTP** (a held control outranks a later stab; falls back on release) so "Voice" is honest. Full polyphony+mixing is a **future direction** (§7).

# 6. UX / engineering-brain review (mockup v4.1) — key outcomes

Two reviewers (operator lens + engineering-reality lens) audited the mockup against the real model. They converged. Headlines:

- **"Voice/newest-held" was a fiction** vs the current engine: one activation per control (stop-then-replace, `lxcontrolservice.cpp:571`), and `resolveValue()` returns the newest-**fired** claim regardless of hold (`fixturechannelcomponent.cpp:36-44`). Decision: **build held-priority** (§5) so the language becomes true.
- **Live-safety blockers** (both lenses): Load vs edit-select was one ambiguous click (also `mActiveProgram` isn't persisted); no All-Stop/panic (no `stopAll()` API); "Output live" was hardcoded/dishonest; Learn had no cancel; Del had no confirm; first-run dead-ended (reintroduced the cross-tab "create a controller first" trap).
- **Engineering realities to budget:** the in-context patch editor sits on the save→hot-reload landmine (resets live animation; param edits don't persist) → **Phase A prerequisite**; "Dup/fork" + "used by N" are real work, not free buttons; the **Controller-device grouping doesn't exist** (cheapest: a `mGroup` label field, not a new resource); Art-Net Mode/Universe/IP are start-time-only (only Frequency/StartChannel/DisplayName are live-safe) and the mock's IP was fabricated; MIDI unplug can't be detected (napmidi has no hot-plug) → show a "last message" activity readout, not a green "connected" chip; OSC isn't in the model yet.

**Fixes applied in mockup v5:** explicit **Load** button + distinct loaded(gold)/editing(teal) states (interactive) + honest Live-Bar program state; always-visible **■ All Stop**; honest **"Output · N held"** indicator + "release all → dark" note; **Learn → Cancel**; **Del → inline confirm**; **⚠ unbound-control** warnings on routing rows + in Controls; **read-only Art-Net** (lock icons, only Refresh editable, IP → "auto (broadcast)"); **"last msg 0.3s"** device activity instead of a "connected" chip; **first-run preview** toggle with empty-state on-ramps; dropped the leaked "v3"; Hold/Latch/Trig tooltips; Test buttons labelled "fires live".

# 7. Future direction — full polyphony + mixing (Option 2)

Held-priority LTP (the committed step) still resolves each channel to **one winner**. Full polyphony makes voices **coexist and mix**, like a real synth. Approach, in build order:

1. **Introduce a Voice object.** Each key-on (Control fire) allocates one or more **Voices** — one per targeted fixture for per-fixture spread — each with its own lifespan (attack → sustain-while-held → release-on-up), reaped when finished (generalize the existing release-linger from per-activation to per-voice). Multiple key-ons → multiple concurrent Voices, even on the same fixture. *This replaces the single-activation-per-control model in `fireTrigger`.*
2. **Per-Voice modulator phase — the hard part / the gate.** Today modulators are per-Effect (one shared `SequencePlayer` graph, `buildModulatorGraph`). Polyphony needs each Voice to run its patch's modulators on its **own** clock from its own key-on, so two held stabs of the same patch aren't phase-locked. The headless curve engine becomes a **template** each Voice samples with its own time (or each Voice gets a lightweight transport). This is the biggest change and should be prototyped first.
3. **A per-channel mixer replaces the single-winner claim.** `resolveValue()` stops returning one claim and instead **mixes** all active Voices on that channel under a selectable **mix mode**, likely **per role**: **HTP** (max) for Dimmer/Strobe intensity, **additive/sum-clamp** for washes/energy, **LTP** (today's latest-wins) and **average/blend** for color crossfades. Roles already exist on channels/params, so per-role defaults are natural.
4. **Voice allocation / polyphony cap (optional).** A per-patch/fixture voice limit with steal rules (oldest/quietest). Probably unnecessary at 3 fixtures, but part of a complete model.

**Cost / risk:** **Large** — reworks the claim stack → a mixer, activations → voices, and per-Effect modulator state → per-Voice; touches `fireTrigger`, `FixtureChannelComponentInstance::resolveValue`, the modulator/SequencePlayer wiring, and the update loop. **Design decisions needed:** per-role mix modes + defaults, release semantics, color mixing (additive vs crossfade), polyphony caps.

**UX payoff:** the Output funnel becomes a genuine **mixer** (stacked contributions, not one winner); "Voices" is literally true; layering/splitting patches across fixtures becomes expressive; the instrument finally sums like a synth. **Staging:** held-priority (committed) is a strict subset — ship it first; then Voice object + per-Voice phase (step 2, the gate) → per-role mixer (step 3, comparatively mechanical) → expose mix mode.

---

## §N — Single/multi unification: the field strip and the Field/Curve split

Shipped on `refactor/single-multi-fx-unification`. Design record: `docs/single-multi-unification-plan.md`.

**The rule that decided the UI: counts live on the rig, never on the effect.** An effect is a function of
normalised position; it neither knows nor states how many sources it will land on. So the patch editor lost
the Spread combo, the "3 voices" chip, the per-fixture meter row and the fixture-name tooltips. Source counts
now appear in exactly two places, both of which are genuinely rig facts: a group's derived
`3 dimmer · 3 strobe · 18 colour` chips in RIG, and the per-fixture strips themselves. Routing rows name the
*group*, not a count.

**The field strip is the one new device.** One `FieldStrip` per source parameter replaces the row of per-voice
meters (already unreadable at 18 sources, impossible at 300). It samples **once per pixel column** — the
decisive case is a hard-edged field: a Chase at PulseWidth 0.05 occupies 1/20th of the strip and a coarser
interpolated sampling smears it or misses it entirely. The Style Guide carries that exact case as a
regression check, plus a moving version. Framed with a `border()` rect, because a field that is legitimately
dark everywhere (a Replace-blend modulator that isn't playing) is otherwise indistinguishable from empty space.

**Field vs Curve is a preview distinction, not a fourth hue.** Both families stay violet. A **Field**
(spatial, analytic: Chase/Noise/Gradient) previews as a strip across position; a **Curve** (temporal,
curve-authored: ADSR/AD/LFO/Step) previews as its shape with a playhead. A `Plate("Field"/"Curve")` names the
family before the kind. Choose the branch by `rtti_cast<FieldModulator>` — enumerating concrete types is how
Gradient silently drew a Curve playhead over its own flat dummy curve.

**A driven input hides its slider.** `InputRow` shows a 4px violet stub and `driven by <modulator>` instead of
a control. That is correctness, not styling: pass 1 of `Patch::update` writes driven inputs every frame, so an
editable slider would visibly fight the modulator. Colour inputs (Gradient's endpoints) get the same treatment
via `ColorInputRow` — a read-only swatch rather than an editable one.

**Order as information.** A group's member chips show their index because order *is* semantics: it sets the
spread direction, so reordering or `Rev` reverses a chase. Reorder is `^`/`v` buttons plus `Rev`, not
drag-and-drop — 1.76 has the DnD API but the agent bridge cannot drive a drag, and this stays verifiable.

## §N+1 — Blend order made legible; folding the params, not the readouts

**The blend modes are named for intent, not arithmetic**: `Set` / `Scale` / `Offset` (was
Replace/Multiply/Add). "Set" carries the warning that Replace never did — it *sets* the value, discarding
whatever came before.

**Order is shown because order is semantics.** Modulators fold into a target in patch order, so each card
carries its evaluation index and a `^` to move earlier (move-up alone reaches any permutation, so one button
beats two). Above the field strip, each source shows its actual assembly: `base -> *1 LFO -> =2 NOISE`. When a
`Set` appears, everything it discards — the base included — is drawn in danger colour with `(=Set drops all
above)`. A wasted modulator is now visible instead of silently doing nothing, which is the failure the old
Replace mode hid.

**Ranges are applied where they cannot be skipped.** `Modulator::value()` is non-virtual and wraps a
protected `rawValue()`, applying `[Min,Max]` around it. Every Field type had overridden `value()` and every
one of them forgot the range, so the Min/Max sliders were inert for Chase, Noise and Gradient. Same lesson as
the Field-preview enumeration: put shared behaviour in the base's non-virtual path, not in a contract
subclasses must remember.

**Clamping moved to the end of the chain.** Clamping after every modulator made `Offset` saturate early and
made Offset-then-Scale lossy in an order-dependent way (base .6 +.7 → clamp 1.0 → ×.5 = .5, versus .65
composed). Every term is already ≤ 1, so one clamp at the end is enough.

**Folding hides the editor, never the readout.** A folded modulator card keeps its family plate, kind, targets
and *preview*; only the editable rows and Blend/Min/Max hide. Sections (CURRENT / SOURCE / MODULATION) fold
the same way. Fold state is a serialized property, so a patch opens the way you left it — deliberately
persisted state rather than session-only UI state, because the alternative is re-folding on every launch.

**Defaults that used to do nothing.** A new Float source defaulted to role `Generic`, which matches no rig
channel — it appeared broken until you noticed the combo. Float now defaults to Dimmer and Toggle to
SoundMode. Modulator names defaulted to the raw RTTI type (`lx::LfoModulator`), which the chain readout
exposed as unreadable; they now derive a short uppercase name (`LFO`).

**A Field modulator owns its transport.** Chase/Noise/Gradient no longer get the five-object napsequence graph
(clock, sink, curve output, player, editor) that every modulator used to receive. They compute from elapsed
time and position, so the player was only ever a clock (Chase) or an "am I playing" flag (Noise, Gradient) --
and all three authored a flat dummy curve nothing sampled, purely to pin `mDuration`. `FieldModulator` now
carries `mElapsed`/`mPlaying` and overrides `isFinished()` as `!mPlaying`, which is the same answer
`!mPlayer->getIsPlaying()` gave. This saves runtime objects, not disk bytes: the graph was never persisted.
The risk was never the deletion but `isFinished()`, because the reap loop uses it to decide when a *releasing*
activation drops its channel claims -- i.e. when live output actually stops.
