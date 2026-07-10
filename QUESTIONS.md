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
