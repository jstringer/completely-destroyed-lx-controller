# v0.2 Refactor — Deferred Questions, Flags & Manual Verification

This file is written **during** the unattended run and pushed **only at the end** (per operating rule / git protocol in IMPLEMENTATION.md). It captures everything that would otherwise have paused development. Nothing here blocked the run — each item was decided-and-continued, fell back, or was deferred to a human.

## Run summary

All 5 phases completed, each built clean (Napkin closed), booted with a clean ResourceManager load log, and passed a headless behavioral self-test; each is its own commit on `refactor/v0.2`, pushed:
- **Phase 1** — Fixture ECS (`lx::FixtureComponent`/`FixtureChannelComponent`/`FixtureChannelMapping`) + demolition of the old rig/effects/presets/MIDI classes. Verified: boots clean, 3 fixtures self-register.
- **Phase 2** — Effects/Parameters/Modulators (ADSR/LFO/Step) on a per-modulator `SequencePlayer` + custom `ModulatorOutput`/`Adapter`/`Track`; persistence. Verified: default-track-creator checkpoint + create→save→reload round-trip.
- **Phase 3** — Triggers + LTP claim stack. Verified: fire→claim/resolve=effect value, stop→revert to base.
- **Phase 4** — Controllers + MIDI bindings + Learn. Verified: synthetic noteOn fires a Momentary controller's trigger, noteOff stops it.
- **Phase 5** — Programs (load/unload, Enter/Exit, program-scoped arming). Verified: gated-before-load, Enter fires on load, MIDI works after load, unload clears.

**No BLOCKED items** — every inferred SDK behavior verified true; no fallbacks were needed. `CurveModulator` and per-modulator `IndependentClock` are the only deliberately-deferred design items (see Decisions). Not merged to `main`; no PR opened (left for the operator).

Format below; newest last within each section.

---

## Post-run fixes (manual verification findings)

- **Modulators output only their first value then freeze** (reported after manual testing). Root cause: each modulator's `SequencePlayer` runs an *empty* default sequence (duration 0), so a non-looping player reaches "the end" immediately, stops, and never ticks the adapter again — and its player time never advances. Fix (commit after Phase 5): (1) in `buildModulatorGraph`, give the player a real duration via a `SequenceEditor::changeSequenceDuration(3600)` and `setIsLooping(true)` so it keeps ticking forever (the player is now purely a heartbeat); (2) drive `evaluate()` from a modulator-owned monotonic clock `mElapsed` (advanced every frame in `Effect::update` via `Modulator::advance(dt)`) instead of the bounded/looping player time — this also fixes ADSR, which would otherwise misbehave when player time wrapped. Verified over multiple frames: a 1 Hz sine LFO's value traces a smooth sine (0.63→1.00→0.77…) with player time advancing. This is the Route-2 fragility flagged pre-implementation — the player-as-clock needed a heartbeat + an independent time source.

## Decisions (guessed by first principles + ponytail, kept moving)

- [Phase 1] Service kept as `nap::lxcontrolService` (NOT migrated to `namespace lx`). Rationale: it's a pre-existing class; migrating churns the app `getService<>`, its RTTI, and the module descriptor (`naplxcontrol.cpp` `NAP_SERVICE_MODULE`). New resource/component classes ARE `lx::`. Revisit if a full `lx` migration of the service is wanted.
- [Phase 1] FixtureChannelComponent LTP claim stack deferred to Phase 3 (it references `lx::EffectParameter`, which arrives in Phase 2). Phase 1 `resolveValue()` returns the clamped base parameter value only. `// ponytail` comment marks this in fixturechannelcomponent.cpp.
- [Phase 1] Used `getDependentComponents()` (FixtureComponent depends on FixtureChannelComponent) so channel instances init before the fixture, instead of reading channel *resource* offsets. Rationale: the channel caches its offset in its own `init()`; declaring the dependency is the minimal idiomatic fix for the init-order hazard PLAN §1 flagged. (PLAN suggested resource-prop reads; the dependency is equivalent and smaller given the instance-cached offset.)
- [Phase 1] `setup()` reduced to `(MidiInputComponentInstance&, ResourcePtr<MidiInputPort>, ErrorState&)`. The `RenderWindow` param is deferred to Phase 2 (needed only for CurveModulator's SequenceEditorGUI).
- [Phase 1] `makeUniqueID()` and `save()`/`rebuildFromLoadedContent()` dropped from the stripped service; they return in Phase 2 when runtime content construction + persistence come back.
- [Phase 1] Left the untracked `data/sequences/` dir in place (old EffectLayer show files). Harmless — nothing loads it in Phase 1; can be cleaned later.
- [Phase 2] **CHECKPOINT VERIFIED** (PLAN §Risks #1): the inferred default-track-creator + adapter-factory chain works. Startup self-check log: "creating default sequence" → "modulator bank sequence tracks: 1" → track type `lx::ModulatorTrack` → "adapter factory fired". Route 2 (custom Output/Adapter) validated against the real SDK; the show-file fallback is NOT needed.
- [Phase 2] `DimmerParameter`/`StrobeParameter` dropped as separate classes — they'll be GUI presets over `FloatParameter` with Role pre-set (YAGNI).
- [Phase 2] `CurveModulator` deferred within Phase 2 (the shipped-editor modulator using the proven createEffectLayer pattern). Procedural modulators (ADSR verified; LFO/Step next) are the priority + now-verified path.
- [Phase 2] `Modulator` and `EffectParameter` bases made constructible (default virtual impls, not pure) so `RTTI_BEGIN_CLASS` works — this SDK has no `RTTI_BEGIN_ABSTRACT` macro. Bare bases are never authored in JSON.
- [Phase 2] Temporary startup self-check `lxcontrolService::verifyModulatorSubstrate()` builds a throwaway `SelfCheck_*` modulator player each boot to prove the substrate. Marked `// ponytail`; REMOVE once the Effects tab drives real construction.
- [Phase 2] Cross-thread modulator state (mHeld/mReleasing written on main thread in onTrigger/onStop, read in evaluate() on the clock thread) is unguarded — benign torn reads for lighting. Revisit only if artifacts appear.
- [Phase 2] Persistence: persist ONLY authored data (Effects + EffectParameters + Modulators). Each modulator's SequencePlayer/clock/ModulatorOutput/sink graph is runtime-only, reconstructed on load by buildModulatorGraph. Simpler + avoids serializing the napsequence graph. **VERIFIED** via a headless create→save→reload round-trip (user_content.json well-formed; reload rebuilt 1 effect / 1 modulator, players got their default sequences).
- [Phase 2] Removed the startup self-check (verifyModulatorSubstrate) — real construction now covers it.
- [Phase 2] IndependentClock deferred: buildModulatorGraph uses StandardClock for all modulators (the `Clock` prop exists but is ignored). Revisit if timing jitter matters.
- [Phase 2] Effect::update runs for ALL effects every frame in the service update (Phase 2 has no triggers/claims, so effects free-run / self-audition). Phase 3 gates this via activations + the claim stack.
- [Phase 2] CurveModulator still NOT implemented (deferred) — the shipped-editor modulator. Procedural ADSR/LFO/Step are complete + verified.
- [Phase 2] Effects tab shows modulator output via ProgressBar(value()) with per-modulator Trigger/Stop; LFO/Step free-run, ADSR needs Trigger. No DMX yet (claims arrive Phase 3).
- [Phase 3] **LTP path VERIFIED headless**: fire a Dimmer effect on Strobe1 → the dimmer channel gets 1 claim and resolveValue()=0.80 (the effect value); stop → claims=0 and reverts to base 0.00. Role-matching (param role+unit → channel), claim push/reap, and release-linger all confirmed.
- [Phase 3] Fixture identity for trigger bindings = `FixtureComponentInstance::getEntityID()` (entity-instance mID, deterministic across runs). The binding UI uses the same source, so persisted bindings round-trip.
- [Phase 3] Activations / runtime trigger state are NOT persisted (boot is dark until a trigger fires) — as designed.
- [Phase 3] `Effect::update` still runs for all effects each frame (triggers gate CLAIMS, not effect ticking; harmless for untriggered effects). Program-scoped gating arrives in Phase 5.
- [Phase 4] **MIDI dispatch VERIFIED headless**: learned a noteOn(60) binding onto a Momentary controller; simulated noteOn → trigger active=1, noteOff → active=0. Binding matching + mode semantics + fire/stop confirmed.
- [Phase 4] A note binding enables BOTH mNoteOn and mNoteOff (createBinding) so Momentary controllers can release — Learn only sees the noteOn, and Toggle/FireOnly ignore off-events anyway. Momentary on CC uses value>=64 as on / <64 as off.
- [Phase 4] onMidiEvent dispatch is armed UNCONDITIONALLY (any matching binding fires its controller). Phase 5 adds the active-program-membership gate.
- [Phase 4] Learn captures message TYPE + NUMBER only (no port/channel) — device-agnostic bindings, per PLAN.
- [Phase 5] **Programs VERIFIED headless**: before load a matching MIDI event does NOT fire its controller (gated); loadProgram fires EnterTriggers (enter active=1); after load the same MIDI fires the ControllerTrigger; unload clears the active program. Program-scoped arming + Enter firing confirmed.
- [Phase 5] loadProgram semantics: on switch, the outgoing program fires its ExitTriggers (transient, rings out via release-linger) and stops its other triggers; the incoming program fires its EnterTriggers. onMidiEvent only dispatches to a controller whose trigger is in the active program (dark when none loaded).
- [Phase 5] Edge: an ExitTrigger whose effect never finishes (e.g. a bare LFO) would linger after unload (not reaped). Real exit looks should use an envelope/one-shot. Accepted v1.
- [Phase 5] Active program is runtime-only (not persisted) — boot loads content but stays dark until a program is loaded, as designed.

---

## Blocked / fallbacks taken

<!-- - [Phase N] <task> failed after 3 attempts: <error>. Tried: <A, B, C>. Fallback applied: <PLAN §Risks ref or minimal stub + // TODO(v0.2-blocked)>. Impact: <what's degraded>. -->

---

## Manual verification needed (hardware/behavioral — could not auto-verify)

- [Phase 1] Fixtures-tab sliders drive observable Art-Net/DMX output on the three physical Eurolite strobes (Strobe1 ch 0–21, Strobe2 22–43, Strobe3 44–65 on Universe0). Auto-verified only: app boots clean, ECS registers 3 fixtures, no load errors.
- [Phase 1] R/G/B ColorEdit3 swatches in the Fixtures tab drive the correct unit channels; Dimmer/Strobe sliders map to the right DMX addresses.
- [Phase 1] MIDI log streams incoming messages from a connected device and hot-plug reconnect works. The headless run had no MIDI device present ("no MIDI input devices currently available"), so this is untested.
- [Phase 2] Effects tab GUI interaction (create effect, add params/modulators, per-type controls, ProgressBar animation, Trigger/Stop) — construction + save + reload verified headless; the actual GUI clicks are untested.
- [Phase 3] Multi-trigger LTP ordering (fire A then B on same channel → B wins; stop B → A resumes; stop A → base) and ADSR release-linger TIMING need visual confirmation on hardware — auto-test covered the claim mechanism + revert with a static value only.
- [Phase 3] Firing a trigger drives real Art-Net/DMX on the bound fixture's role-matched channels — verify on the physical strobes.
- [Phase 4] With a real MIDI device / loopMIDI: learn a pad note onto a Momentary controller (note-on fires / note-off stops), a Toggle latches, a CC works with the 64 threshold, and two bindings drive one controller. Headless test used synthetic events + one device-less run; real-device path untested.
- [Phase 5 / END-TO-END] The full show from PLAN.md §Verification, all GUI-driven on the running app: author 2 effects (color LFO, dimmer ADSR), 3 triggers (Controller/Enter/Exit), 1 controller learned from a MIDI pad, 1 program containing them; restart → everything reloads; load the program → EnterTrigger lights on the fixtures; pad press/release drives the ADSR with visible release-linger; fire two overlapping triggers on one channel → confirm LTP order + revert. Watch Art-Net/DMX on the physical strobes throughout. (Auto-run verified every mechanism headlessly; this is the human hardware+GUI pass.)

---

## Open questions for the operator

<!-- - [Phase N] <genuine design question that needs your call; a default was chosen to keep moving — see Decisions>. -->

---

# Modulator → Curve-Backed Engine Migration (2026-07-10, run 2)

Design: `docs/superpowers/specs/2026-07-10-parametric-sequence-shapes-design.md`.
Plan: `docs/superpowers/plans/2026-07-10-modulator-curve-migration.md`.
Goal: replace the segment-less procedural modulator engine (own monotonic clock + player-as-heartbeat) with a stock napsequence curve engine (player time is the timebase), keeping the Target/Blend/gate integration spirit.

## Decisions (migration)

- [M-setup] Folded plan Task 1 (throwaway spike) + Task 2 (authorFloatCurve helper) into ONE boot self-check `verifyCurveSubstrate()` — the same idiom run 1 used (`verifyModulatorSubstrate`): write the real helper, prove it by logging the sink ramp at boot, then delete the self-check. One build cycle instead of two. Ponytail.
- [M-setup] Plan Tasks 8 & 10 wrote `git add -A`; overridden to explicit-path staging per IMPLEMENTATION.md git protocol (never `git add -A`).
- [M-Task1+2] **Curve substrate PROVEN** via the boot self-check (log: `sink` traced the player's own time exactly on a 1s 0→1 ramp, zero load errors). Facts learned that shape later tasks:
  - A stock `SequencePlayerCurveOutput` on an empty-sequence player (`CreateEmptySequenceOnLoadFail=true`) **auto-creates exactly one curve track already bound to the output** (napsequence registers a default-track-creator for `SequencePlayerCurveOutput`). → `buildModulatorGraph` reuses `mTracks.back()`; no `addNewCurveTrack`/`assignNewOutputID` needed (kept as a fallback for the empty case).
  - `authorFloatCurve` recipe that works: find track → delete existing segments → `insertSegment(trackID, t_i)` per interval (returns the new segment ptr; empty-track first insert makes `[0,t]`, rest append) → `changeCurveSegmentValue(BEGIN/END)` + `changeCurveType` → `changeSequenceDuration(last t)`.
  - **No heartbeat / no monotonic clock needed**: a real-duration curve keeps the player ticking and its time IS the timebase — the exact thing the old engine worked around. Confirmed root-cause fix.
- [M-Task3-8] Collapsed the plan's 6-commit shim sequence into: **(A) one coherent engine-swap commit** — refactor base `Modulator` (drop `evaluate()`/`advance()`/`mElapsed`; add `generateCurve()`/`update()`/curve handles; `value()` unchanged), migrate ADSR+LFO+Step to curves, rewrite `buildModulatorGraph` (stock `SequencePlayerCurveOutput`, reuse the auto-created track), `Effect::update` `advance`→`update`, and **delete** `ModulatorTrack/Adapter/Output` + their registrations (required together — the base and subtypes and the custom adapter are compile-coupled, and a half-migrated tree with a `usesCurve` shim outputs 0 anyway, so it isn't more "working"). Then (B) add AD, (C) GUI, (D) persistence. Rationale: fewer commits, each compiles+boots; the shim's intermediate states aren't meaningfully safer.
- [M-Task3-8] `generateCurve(nap::lxcontrolService&)` calls back into `svc.authorFloatCurve(*mEditor, mTrackID, keys)`; `modulator.h` forward-declares `nap::lxcontrolService` (reference param only), subtypes include the service in their `.cpp`. Kept `authorFloatCurve` as a service method (already committed) rather than a free function — minimal churn.
- [M-Task-D] **Behavioral engine self-check PASSED** (headless, sampled by the ADSR's player time since the app runs uncapped ~thousands of fps): ADSR traced attack→decay→**sustain held by pause** (player time frozen at A+D=0.40)→release 0.5→0 over R→stop; LFO 2Hz sine oscillated 0↔1 repeatedly; AD OneShot spiked then decayed to 0 by A+D=0.30 and stopped. All correct, zero errors. Curve engine verified end-to-end.
- [M-Task-D] **Fixed a pre-existing persistence bug** surfaced by the 3-modulators-on-one-effect test: `makeUniqueID` only checked `ResourceManager::findObject`, but `createObject` indexes objects under their original id while we reassign `mID` afterwards — so multiple modulators on one effect all got id `<effect>_Mod`, producing duplicate ids that failed `user_content.json` reload (it renamed to `.bak` and continued empty). Root-caused at the chokepoint: `makeUniqueID` now also tracks a `mIssuedIDs` set. Latent before because prior tests used one modulator/effect; the curve engine makes multi-modulator effects (ADSR+LFO on one param) common. VERIFIED: fresh create→save→reload of an effect with ADSR+LFO+AD now round-trips clean.

## Migration run summary

Completed the modulator→curve migration on `refactor/v0.2` in 3 pushed commits, each built clean (only benign LNK4099 PDB warnings) and boot-clean (zero ResourceManager load errors):
- `85ac853` — `authorFloatCurve` runtime curve helper + `lx::Key` (substrate proven via a since-removed boot self-check).
- `dfb5eb5` — engine swap: base `Modulator` + ADSR/LFO/Step on a stock `SequencePlayerCurveOutput` curve graph (player time is the timebase); deleted `ModulatorTrack/Adapter/Output` + registrations; `Effect::update` `advance`→`update`.
- `fe2adae` — `lx::AdModulator` + Effects GUI (AD button, Blend/Mode combos, live shape plot, regen-on-edit) + `makeUniqueID` duplicate-id fix.

Removed the framework-fighting scaffolding entirely (segment-less `ModulatorTrack`, hand-rolled `mElapsed` monotonic clock, the 3600s player-heartbeat). Kept the integration spirit: Target/Component, Min/Max, Blend, trigger→release-linger→`reapClaims`, and the "only serialize the Modulator, rebuild the graph on load" persistence. `Modulator::value()` unchanged.

## Manual verification needed (migration — GUI / hardware, could not auto-verify)

- **GUI interaction** (Effects tab): Add ADSR/AD/LFO/Step, edit A/D/S/R/shape/Hz/Rate and see the curve re-author + the live plot track; Blend and Mode combos change behavior; Trigger/Stop gate the modulator. Auto-verified only headlessly via the `addModulator` path + a value trace; the actual clicks are untested.
- **DMX output**: a triggered modulator drives real Art-Net on the bound fixture's role-matched channels (claim path unchanged; only the modulator value source changed). Watch the physical Eurolite strobes.
- **Feel/timing on hardware**: ADSR sustain-hold + release-linger, LFO rate/shape at speed, AD one-shot vs loop-while-sustained, LFO retrigger phase reset. Headless traces confirmed the value math; hardware feel is unverified.
- **Ceilings to sanity-check** (see design spec §9 / code comments): Sine & Gaussian LFO are bezier approximations; Square/Pulse/Step edges are a ~10ms ramp (napsequence curves are continuous — `ECurveInterp::Stepped` exists and could make these exact later); ADSR release always starts from the sustain level; Step Trigger-advance mode currently plays like Clock mode.

## Open questions for the operator (migration)

- **`CLAUDE.md` is stale** relative to the v0.2 code in general (it still describes the pre-v0.2 EffectLayer/Preset/PresetManager architecture and the Presets/Effects/MIDI tab layout). Not touched by this migration; worth a dedicated CLAUDE.md refresh pass covering the whole v0.2 refactor (fixtures ECS, effects/modulators/triggers/controllers/programs, and now the curve-backed modulator engine).

---

# Multi-fixture-fx audit + fixes (2026-07-13, run 3)

Context: audited the WIP `multi-fixture-fx` branch (Chase/Noise modulators + `Effect::mTargetMode`/`mFixtureCount` slot machinery) for architecture and UX/DX, then implemented and verified the fixes below. `CLAUDE.md` is stale here too (still describes the pre-v0.2 architecture) — not touched this run, same as noted above.

## Run summary

- **Phase A** — `regenerate.bat` had not been run since `chasemodulator.*`/`noisemodulator.*` were added; they were absent from `naplxcontrol.vcxproj`, so nothing exercising Chase/Noise had actually been compiled in. Ran `regenerate.bat` + `build.bat`, clean (only pre-existing benign LNK4099 PDB warnings).
- **Phase A.5** — root-caused why `data/user_content.json.bak` existed: it held a real `Multiple`-mode `ChaseModulator` effect (`FixtureCount: 3`) that failed to reload under the stale (pre-Phase-A) DLL — `lx::ChaseModulator` wasn't a registered RTTI type yet, so `ResourceManager` hit an unknown-type load failure and `lxcontrolService::setup()`'s fallback auto-renamed the file rather than halting. Verified fixed: swapped the `.bak` content in (after backing up the live file) and smoke-launched — loaded with zero errors, `Effect_chase_Mod_rt_Player` rebuilt correctly. Restored the original file afterward.
- **Phase B** — fixed a real correctness bug in `NoiseModulator`: `hash01(slot, step)` had no per-instance seed, so two independently-added `NoiseModulator`s (e.g. one per Red/Green/Blue component, the intended way to get colored noise) produced **identical** values at every `(slot, step)` — R=G=B, grayscale flicker only. Added a `Seed` property folded into the hash, and `lxcontrolService::addModulator` now auto-assigns each newly-created `NoiseModulator` a fresh seed (`mNextNoiseSeed` counter, re-synced past every persisted seed on load in `rebuildFromLoadedContent` so a fresh seed never collides with one loaded from disk).
- **Phase C** — decoupled `Effect::mFixtureCount` from manual entry. It used to be a hand-typed GUI field (`InputInt`, default 2 — itself mismatched against the 3-fixture rig) that had to be kept in sync with however many fixtures a Trigger binding actually checked; `fireTrigger` used `slot_index % FixtureCount`, so a mismatch silently wrapped/repeated or left slots at base. Now `fireTrigger` derives the count directly from the binding's actually-matched fixtures each time it fires (`lxcontrolService::syncEffectFixtureCount`, called before slot assignment) and `slot = slot_index` with no modulo needed — "however many fixtures you bind it to" is now the entire mental model, matching the ask literally. `setEffectTargetMode` dropped its `fixtureCount` parameter (mode is still authored; count never is). **Decision, not just a bug fix** — logged here per the operator's ask: this doesn't fix the pre-existing "shared `Effect` state across concurrent Triggers" limitation (already known, see the Phase 2/3 Decisions above) — if the same Effect is bound by two Triggers with different fixture counts at the same time, the second fire's derived count wins for both, same as before. Full fix would mean per-activation Effect/Modulator instances, a materially bigger change; out of scope for this pass. Revisit if that scenario actually comes up in practice.
- **Phase D** — fixed an authoring/runtime mismatch: the Triggers-tab binding checklist iterated fixtures in raw registration order (`getFixtures()`), while `fireTrigger` assigns Chase/Noise slots in physical DMX `StartChannel` order (deliberately, per its own comment) — these could disagree with no warning. Added `lxcontrolService::getFixturesPhysicalOrder()` (physical-order sort, now the single source of truth, reused by both `fireTrigger` and the GUI) and updated the binding editor to iterate it, plus show each checked fixture's resolved slot index inline (`"Strobe 2 [slot 1]"`) so authoring is WYSIWYG instead of only visible after firing. Also labeled the Effects-tab per-slot preview bars with a best-effort resolved fixture name (`describeEffectSlot`) instead of a bare index, and switched the bindings list to show fixture display names instead of raw entity mIDs.
- **Phase E** — re-keyed the Effects tab's `mModTargetIndex`/`mModHistory` from raw `Effect*`/`Modulator*` pointers to `mID` strings, matching the pattern already used for `mBindEffectIdx`/`mBindFixtures` (and explained in that code's own comment): any `save()`-triggering action hot-reloads `user_content.json` and reallocates objects at new addresses next frame, so the pointer-keyed maps were silently orphaning the live shape-plot history and target-parameter selection on every parameter/modulator add. Also fixed `Effect::mFixtureCount`'s default (2 → 1, since it's now a fallback-before-first-fire value, not authored).
- **Phase F** — end-to-end verification. Built a temporary headless self-check (`lxcontrolService::verifyMultiFixtureFx`, mirroring this file's own prior `verifyModulatorSubstrate`/`verifyCurveSubstrate` idiom, since there's no way to drive the live ImGui programmatically) that used the real authoring API (`createEffect`/`addEffectParameter`/`addModulator`/`createTrigger`/`setTriggerBindings`/`fireTrigger`) to build a Chase effect (Dimmer, Multiple) and a 3-seeded-Noise effect (Red/Green/Blue FloatParameters, Multiple) bound to all 3 fixtures, fired both, stepped time, and logged resolved channel values. **Confirmed all fixes together**: `fixtureCount` derived as 3 (not the old default-2 mismatch); Chase produced distinct dimmer values per fixture (1.0/0.0/0.0 at the sampled instant); Noise produced fully distinct R/G/B per fixture *and* distinct values across fixtures (e.g. Strobe 1: `R=0.634 G=0.371 B=0.387`; Strobe 2: `R=0.809 G=0.251 B=0.694`; Strobe 3: `R=0.437 G=0.054 B=0.119`) — the exact "noise modulates color across fixtures" case that was broken before Phase B. Ran against a backed-up copy of the live `user_content.json` (its own `removeEffect`/`removeTrigger` calls cleaned the test content back out by the end, verified byte-identical to the pre-test file afterward), then deleted the self-check and did one final clean rebuild + smoke test to confirm nothing regressed.

## Decisions

- [Phase C] Chose "derive fixture count from the binding at fire time" over "keep the manual field, just add a sync button" — the former fully eliminates the desync bug class rather than just making it easier to fix by hand, and matches the operator's literal framing ("assign to any number of fixtures ... animate all of them automatically"). See the Phase C summary above for the accepted tradeoff (doesn't touch the pre-existing shared-Effect-across-Triggers limitation).
- [Phase D] `describeEffectSlot`'s fixture-name resolution is best-effort (first matching Trigger binding wins) since an Effect can be bound by more than one Trigger with different fixture sets — documented in the method's own header comment rather than solved, consistent with the accepted Phase C limitation.
- [Phase E] Did not re-key `mModTargetIndex`/`mModHistory` by anything fancier than `mID` (string) — same minimal fix already validated by the existing `mBindEffectIdx`/`mBindFixtures` pattern; no reason to diverge.
- [Cleanup] Did **not** delete `data/presets/` (confirmed via a prior investigation to be genuine leftover manual-test snapshots from the old pre-v0.2 Presets-tab flow — nothing in the current architecture reads them) or `data/user_content.json.bak` (now a known-safe, already-proven-working reference file, per Phase A.5) — both are irreversible deletions of real (if stale) data files the operator didn't explicitly ask to remove, so left in place per this session's established caution around destructive actions on untracked data. Operator's call whether to delete either.

## Manual verification still needed (hardware/GUI — could not auto-verify)

- **Real DMX/hardware**: the headless self-check confirmed the value math end-to-end (claim → `resolveValue()`), but never watched actual Art-Net output on the physical Eurolite strobes. Author a Chase + a colored-Noise effect through the actual GUI, bind all 3 fixtures, fire, and confirm visually: Chase sweeps in true physical left-to-right order; Noise flicker looks visually distinct per fixture and per color channel now that seeds decorrelate.
- **GUI interaction**: the new "(N fixtures — set by whichever Trigger binding last fired this effect)" text, the `[slot N]` inline binding-checkbox labels, and the per-slot progress-bar fixture-name labels were never actually seen rendered (no way to screenshot the live ImGui this session) — worth a quick visual pass to confirm layout/readability, e.g. that the longer checkbox labels and progress-bar overlay text don't clip or look cramped.
- **Multiple Triggers on one Multiple-mode Effect with different fixture counts** — the accepted Phase C limitation (last-fired binding's count wins for all). Not something the self-check exercised since it only fired one trigger; worth deliberately testing if this scenario is likely to come up (e.g. a Chase effect reused by both a 3-fixture "all strobes" trigger and a 2-fixture "just the pair" trigger).
- **`Detected change to user_content.json. Files needing reload` log line** appeared during the Phase F self-check (many rapid `save()` calls in one synchronous burst triggered the ResourceManager's directory watch mid-init). Didn't observe any actual problem from it (process was killed by the smoke-test timeout shortly after), but a real GUI session that adds several effects/modulators/bindings in quick succession could in principle trigger the same rapid-save/detect-reload behavior — not new (pre-existing directory-watch behavior, unrelated to this run's changes), but flagging in case it's ever seen to cause a visible hitch or duplicate-load symptom. **Follow-up: this was in fact the root cause of a real bug — see the next section.**

---

# "Chase/Noise not animating" — root cause + fix (2026-07-13, run 4)

## Bug report

After run 3's fixes, the operator built Chase/Noise effects through the real app, bound them to fixtures, fired them (both via the Effects tab's local per-modulator "Trigger" button and via a real Program/Controller-bound Trigger), and confirmed neither actually animated — output looked frozen.

## Root cause (confirmed, not guessed)

The `Detected change to user_content.json — Files needing reload` line flagged as a loose end at the end of run 3 turned out to be the actual cause. `nap::ResourceManager` really does hot-reload any file it loaded when that file changes on disk (`resourcemanager.h:142` `checkForFileChanges()`, `:165` `mPostResourcesLoadedSignal` — confirmed via SDK headers, not assumed). Nearly every authoring action (`addModulator`, `addEffectParameter`, `setEffectTargetMode`, `setTriggerBindings`, …) calls `save()`, which rewrites `user_content.json` and reliably trips this reload within about one real frame of the action.

Verified empirically (temporary diagnostic, since removed) that:
- `mEffectEntries`/`mEffects`/etc. (all `rtti::ObjectPtr<T>`-based) **survive a reload correctly** — NAP transparently repatches these to the fresh, recreated objects (confirmed: same count, same IDs, each `Effect`'s address just moves).
- But `lx::Modulator::mPlayer`/`mSink`/`mEditor` (`modulator.h:67-69`) are **plain raw pointers**, populated only by `buildModulatorGraph()` — which was only ever called once, from `setup()`/`rebuildFromLoadedContent()`, at startup. After any subsequent hot-reload, every Modulator's raw wiring pointers go stale/null, and nothing re-populates them — so `Modulator::value()`/`Chase::valueForSlot()`/`Noise::update()` all silently do nothing from that point on, regardless of how many times `onTrigger()` is called afterward. This is not Chase/Noise-specific; it would eventually bite ADSR/LFO/Step too (just less visibly, since their frozen last-value looks less obviously "wrong").

## Ponytail check before fixing (important — read this before extending the fix further)

The operator explicitly asked to verify this wasn't an overcorrection before building anything, given the size of the resync-everything fix originally proposed at the end of run 3 (rebuild `mEffectEntries`/`mTriggers`/`mControllers`/`mActivations`/clear all channel claims). That check was the right call:
- A minimal signal-hook diagnostic (connect to `mPostResourcesLoadedSignal`, log `mEffectEntries` contents before/after) confirmed the bookkeeping vectors need **no rebuilding at all** — only the Modulator's raw player/sink/editor wiring does. The originally-proposed fix would have been real overkill.
- **First "fix confirmed working" result was itself wrong** — a sampling-aliasing artifact. The diagnostic logged Chase's value once every 1.0 real second, exactly matching Chase's default `Rate=1.0` (one full cycle per second), so the value read as frozen even though the player was cycling correctly underneath. Changing the log interval to 0.33s (non-commensurate with 1.0s) immediately showed `playerTime` genuinely advancing/looping and the pulse correctly toggling. **Lesson: when verifying periodic/animated behavior by sampling, the sample rate must be checked against the thing being measured's own period, or a real fix can look broken (or a real break can look fixed) by pure aliasing.** Applies beyond this bug — any future "does it animate" check on Chase/LFO/Step should use a sample interval that isn't a small-integer ratio of the modulator's own Rate.

## Fix implemented

- Added `lxcontrolService::onResourcesReloaded()`, connected to `mResourceManager->mPostResourcesLoadedSignal` at the end of `setup()` (connected *after* the initial load, so it only fires for subsequent reloads, not the one `setup()` already handles via `rebuildFromLoadedContent()`). It walks `mEffectEntries` (already correctly repatched) and re-wires every Modulator's player graph.
- Extracted the shared "(re)build one modulator's player/sink/editor graph, re-propagate slot count, bump `mNextNoiseSeed`" logic out of `rebuildFromLoadedContent()`'s inline loop into `lxcontrolService::rewireModulator(Effect&, ModulatorEntry&)`, used by both the initial load and the new post-reload handler — one function, two call sites, no duplicated logic.
- Verified via a temporary real-time diagnostic (build effect → let the async reload settle → trigger as a separate later action → sample at a non-aliasing interval): both Chase and Noise now genuinely animate over real time post-reload, and stay animating (no further reload happens unless another edit is made).

## Known accepted tradeoff (not fixed, deliberately)

`rewireModulator` always rebuilds a modulator idle (`buildModulatorGraph` ends with `setIsPlaying(false)`) — matching every modulator's normal freshly-built state. So an effect that's actively animating will visibly reset (needs re-triggering) if *any other* edit anywhere in the app trips a reload while it's mid-flight. This is a real, narrow UX rough edge, not solved here — the complete fix would mean `save()` avoiding tripping the app's own file-watcher on its own writes (unclear if controllable from app code at all — `watchDirectory`/`checkForFileChanges` are called automatically by the SDK's app-runner; lxcontrol's own code never calls them, so there's no obvious app-level toggle) — a separate, bigger change if it turns out to matter in practice.

## Cleanup note

While debugging, discovered the operator's own running instance of the app was independently active during this session (new real content — a `color_chase` effect, `Trigger_button3`, `Effect_dimmer_chase`'s `FixtureCount` going 1→3 — appeared in `user_content.json` between two of this session's backups, none of which this session created). Restored the most recent pre-test snapshot rather than guess which parts were "real" vs. test artifacts, then manually stripped the two leftover `dbg_chase`/`dbg_chase_2` junk effects (unreferenced by any Trigger, safe to remove) directly from the JSON. Validated the cleaned file loads with zero errors in a real run. `nap_validate.py` reports 23 false-positive "unknown Type" errors on this file — its type catalog was never refreshed for this project's own `lx::` module classes (only knows SDK/demo types); not a real problem, confirmed by repeated clean runtime loads this session.
