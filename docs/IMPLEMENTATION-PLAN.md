# lxcontrol GUI/UX Rebuild — Implementation Plan

Execution playbook for building the redesign in `docs/gui-refactor-findings.md` (design intent + §5 locked decisions + §6 review + §7 polyphony). This is the *how*: concrete phases, files, verification, and commit points. In-repo, in-line development on `refactor/v0.2`, **a commit at the end of every phase** (and every safe sub-step).

**Mode:** R&D / prototyping. First-principles over "it wasn't in the plan." When a phase needs engine architecture that doesn't exist yet (held-priority arbitration, `stopAll`, per-voice modulation, a Controller device-group, a migration loader), audit the current model, then build it — don't route around it. Record unknowns inline as `UNKNOWN:` and pick the first-principles default to keep moving.

**Build discipline (per `nap-build-run-verify`):**
- `regenerate.bat` after adding any new `.cpp`/`.h` to `module/src/` or changing `RequiredModules` — CMake captures the file list at regenerate time; a new source left out compiles to nothing (symptom: `Unknown object type` at load).
- `build.bat` to compile. Close Napkin first (holds a lock on `naplxcontrol.dll` → `LNK1104`).
- Run the app to verify load + behavior; `nap-validate-data-json` after any `objects.json` edit.
- Keep a captured copy of a real `data/user_content.json` as a **migration test fixture** before any rename/loader change (Phase C/D). Copy → `test/fixtures/user_content.pre-rename.json`.

**Legend:** ✅ backed by current API · 🔧 needs new engine code · ⚠️ risk/unknown.

---

## Phase 0 — Baseline & safety net  *(commit: "chore: refactor planning docs + baseline")*
- Commit the design record (`docs/gui-refactor-findings.md`, this plan, the reference screenshots).
- Capture the current `data/user_content.json` as a test fixture; add `data/user_content.json` to `.gitignore` (per-user runtime state, not source — CLAUDE.md: "delete to reset").
- Confirm a clean baseline build (`build.bat`) + run before touching code, so every later failure is attributable.
- ⚠️ `UNKNOWN:` whether the toolchain builds cleanly in this environment — establish now.

## Phase A — Own persistence  🔧  *(the prerequisite; deletes code, kills the live-edit bugs)*
Goal: stop routing authored content through the ResourceManager's directory-watch. Own the load; make `save()` a debounced dirty-flush; delete the hot-reload/rewire path; persist the active program; make direct edits persist.
- **A1. Own the load.** Replace `loadFile`-based load of `user_content.json` in `setup()`/load path with manual `rtti::readJSONFileObjects` + `rtti::DefaultLinkResolver::sResolveLinks`, then hand-init leaves-first (mirror the existing `createEffect`/`buildModulatorGraph` init ordering). File is never handed to the ResourceManager → never watched → never hot-reloads.
- **A2. Delete the reload machinery.** Remove `onResourcesReloaded()`, the `mResourcesReloadedSlot` connect, `rewireModulator`'s reload call-site (keep it for initial build), and the broad `getObjects<T>()` re-scan in `rebuildFromLoadedContent`. Net-negative LOC.
- **A3. Debounced dirty save.** `save()` → set `mDirty`; flush in `update()` at most ~every 500 ms and on shutdown/focus-loss. Because the write no longer triggers a reload, **all** mutations (including the direct field edits at `lxcontrolapp.cpp:325-337`) just set `mDirty` → uniform persistence, no per-widget `save()` sprinkles.
- **A4. Persist active program.** Serialize `mActiveProgram` mID; restore in `setup()`. Fixes "restart → nothing loaded."
- ✅ Verify: author an effect + modulator, edit a param while it animates (no reset), restart (content + active program restored), an unrelated edit mid-animation doesn't reset the playing modulator.
- ⚠️ Risk (highest): you now own cross-object link resolution (Program→Trigger→Effect, ControllerMapping→Control/Trigger, Modulator→Param). `DefaultLinkResolver` does it — wire it correctly once. Keep the `.bak`-on-failure guard so a bad file can't halt startup.
- *Commit: "feat(persist): own user_content load + debounced save, drop hot-reload"*

## Phase B — GUI architecture split + design-system hooks  🔧  *(no behavior change; enables everything visual)*
- **B1. View modules.** Break `lxcontrolapp.cpp`'s one `update()`/`drawMainUI()` into per-screen structs (`RigView`, `ProgramsView`, `ControlsView`, `PerformView`) each with `draw(UiContext&)`, owning their own mID-keyed scratch state. `UiContext` carries `lxcontrolService&`, `IMGuiService&`, `const Palette&`, shared mode/learn state.
- **B2. Theme as data.** Add a `*ServiceConfiguration` JSON with `IMGuiServiceConfiguration` (the "terminal/luminous" palette from findings §5: teal/cyan accent, gold live, violet mod; mono font, size, `Scale` via `getScale()`), point `app.json` `ServiceConfig` at it. Verify the exact `Custom`-palette property names against a working config (`nap_usage.py nap::IMGuiServiceConfiguration`); fall back to `HyperDark` + per-widget overrides if `Custom` fights.
- **B3. `src/lxtheme.h`** — semantic wrappers over `getPalette()` (`accent/live/mod/danger/muted/pulse`); replace every hard-coded `ImVec4`. **~5 widget helpers** (`SectionHeader`, `DangerButton`, `DisabledRegion` (the 1.76 alpha-gray-out), `LabeledCombo`, `ModPlot`).
- **B4.** Delete the dead `nap::ParameterGUI` resources (`objects.json:1663-1680`).
- ✅ Verify: identical behavior, new theme, no literal colors left in app code. `nap-validate-data-json` on the config + objects.json.
- *Commit: "refactor(gui): split views + UiContext, add theme config + lxtheme"*

## Phase C — Engine truths for an instrument  🔧  *(make the model back the mockup's promises)*
Audit-then-build, first-principles. Each is independently committable.
- **C1. `stopAll()` / panic.** New service method: walk every activation, `effect->stop()`, reap all claims, clear activations. Backs the Live-Bar **■ All Stop** and Perform Blackout. Small.
- **C2. Held-priority arbitration** (the committed "Voice" semantics). Today `resolveValue()` returns the newest-*fired* claim (`fixturechannelcomponent.cpp:36-44`), hold-agnostic. Change: a claim from a *currently-held* control outranks a non-held (stab) claim; among held, newest wins; on release, fall back to the next-highest still-held, else base. Requires the claim/activation to carry a "held" flag updated from the Control's Momentary/Latch state in `onMidiEvent`. Audit `resolveValue` + `Activation` + the claim struct first. ⚠️ Changes show behavior — this is the point.
- **C3. Honest output state.** Expose held-voice count / per-channel winning claim from the service so the GUI can render "dark / N held / loaded-idle" and the real Output mix (not a phantom one). Backs the review's #6.
- **C4. Controller device-group.** Add `std::string mGroup` (device label) to `Controller` (`controller.h`); group by it in the Controls UI. Presentational, migration-safe (absent = ungrouped). *Not* a new resource (findings §6). The "device" is the group; Controls are its members.
- **C5. Patch duplicate + "used by N".** `duplicateEffect()` = deep-copy Effect + params + modulators, rebuild each modulator graph (reuse `buildModulatorGraph`). `effectConsumers(effect)` = scan every `Trigger.mBindings` + `Program` mapping → reverse index for the "used by N / fork" affordance. Budget: dup is the most complex "small button."
- *Commits: one per C-item, e.g. "feat(engine): stopAll", "feat(engine): held-priority arbitration", …*

## Phase D — Model rename + migration loader  🔧  ⚠️ *(touches existing authored data)*
- **D1. Migration map at the load boundary** (built on Phase A's owned loader): a one-time type-string substitution over the JSON text before deserialize — `lx::Effect→lx::Patch`, `lx::EffectParameter→lx::PatchParameter`, `lx::Controller→lx::Control`, `lx::{Enter,Exit,Controller}Trigger→lx::Trigger` (+ inject a `Kind` field), `Slot→Voice` where surfaced. Idempotent; upgrades `user_content.json` on first launch.
- **D2. Rename the classes** (`effect.h→patch.h`, etc.), update `RTTI_*`, includes, and all call sites. Collapse the 3 empty Trigger subtypes to `Trigger{ ETriggerKind Kind }`.
- ✅ Verify: unit-migrate the captured fixture `test/fixtures/user_content.pre-rename.json` → loads clean; keep the `.bak` guard; rename in one pass (never partial).
- ⚠️ Risk (high): a wrong map wipes user data via the `.bak`-and-empty path. Test migration on the real fixture *before* shipping.
- *Commit: "refactor(model): rename Effect→Patch/Controller→Control + migration loader"*

## Phase E — The new UI  🔧  *(the mockup, made real; regenerate per new view file)*
Build the four surfaces from mockup v5 on the Phase-B scaffold + Phase-C/D engine. Sub-commits per surface.
- **E1. Live Bar + Perform/Edit mode** — persistent bar (loaded program or "— none —", honest output state via C3, ■ All Stop via C1, MIDI-activity readout, mode toggle). Perform = play-only pad grid keeping bar + output readout visible (review #5).
- **E2. PROGRAMS 3-panel** — Programs list (explicit **Load** vs select-for-edit, distinct loaded/editing states, active-program from A4) | routing rows (Control→Patch→Fixtures via existing `ControllerMapping`+`Trigger`, unbound-control ⚠, Test-fires-live, inline "+ new control/patch") | in-context Patch editor (shared, C5 dup/fork, playhead previews). Automatic on-load/exit rows. Output mix from C3.
- **E3. CONTROLS** — device-grouped controllers (C4), Hold/Latch/Trig + tooltips, **Learn with Cancel/Esc**, activity ("last msg") not a fake "connected", Del confirm.
- **E4. RIG** — read-only Art-Net (only Frequency/StartChannel/DisplayName editable; universe/mode/IP restart-only), fixture strips (faders fill, swatches, output LEDs).
- **E5. First-run empty states + destructive-action confirms** across surfaces.
- ✅ Verify: run the app, exercise each review fix (Load≠blackout, All-Stop, honest output, Learn cancel, Del confirm, first-run path).
- *Commits per E-item.*

## Phase F — OSC as a second input  🔧  *(model is already input-agnostic)*
- Add `OscBinding` (own `matches(OSCEvent)` + `Control` ref) beside `MidiBinding`; `mOscBindings` + `onOscEvent()` reusing the identical Control→routing path; add `naposc` + an OSC receiver to `objects.json`; extend Learn to accept OSC. `regenerate.bat` (new module + `.cpp`).
- *Commit: "feat(input): OSC bindings alongside MIDI"*

## Phase G — Full polyphony + mixing  🔧🔧  *(future; findings §7)*
Voice object → per-Voice modulator phase (the gate) → per-role mixer (HTP/additive/LTP/blend) → optional voice cap. Large; separate effort. Held-priority (C2) is a strict subset shipped first. Not started until E is stable.

---

## Cross-cutting
- **Regenerate triggers:** any new `.cpp` in `module/src/` (Phases C4, D, F) → `regenerate.bat` before `build.bat`.
- **CLAUDE.md is stale** (documents `Preset`/`MidiMapping`/`EffectLayer`/timeline that don't exist). Refresh it at the end of Phase E to match the shipped model.
- **Unknowns register** (resolve first-principles, record here as they arise): toolchain build health (Phase 0); exact `IMGuiServiceConfiguration` Custom-palette JSON shape (B2); whether `ArtNetController` supports a clean stop/restart for live Universe/Mode edits (kept restart-only for now, E4); held-flag plumbing granularity for C2.
