# v0.2 Refactor — Autonomous Implementation Runbook

Companion to **PLAN.md** (the design). PLAN.md = *what* to build (§§1–11) and its risks/fallbacks; this file = *how* to drive it to completion **unattended, in auto mode, with no human present**. If the two ever conflict, PLAN.md wins on design and this file wins on process.

---

## PRIME DIRECTIVE

**Always move development forward.** Never stop, never wait, never ask. The operator is away from the machine. Every turn ends with either committed progress or a logged deferral **and** the next task already started. When in doubt, decide and continue — a logged wrong guess that keeps moving beats a correct question that halts.

---

## Operating rules — read at the start of every turn

1. **No pausing, ever.** Do **not** call `AskUserQuestion`, `ExitPlanMode`, request review, or otherwise block on the user. If you feel the urge to ask: instead decide by first principles + ponytail, write one line to `QUESTIONS.md`, and continue in the same turn.
2. **Decisions:** any ambiguity, missing detail, or fork → pick the simplest option consistent with PLAN.md's intent and its stated fallbacks. Log it under `QUESTIONS.md` → *Decisions*. Do not deliberate in prose — decide and move.
3. **Build failures are normal work, not a stop condition.** Read the compiler error / the ResourceManager load log, invoke the `nap-build-run-verify` skill, fix, rebuild. Known traps: run `regenerate.bat` after adding/removing any `.cpp`; **close Napkin** if `LNK1104 ...naplxcontrol.dll`; `Unknown object type` = missing regenerate or bad RTTI; `Encountered embedded pointer...` = Embedded/Default flag mismatch; unresolved externals = a `.cpp` left out of the CMake file list (regenerate).
4. **Blocked task** (only after **3** genuine fix attempts): write a `BLOCKED` entry to `QUESTIONS.md` (what failed, what you tried), then in priority order: (a) apply PLAN.md's named fallback for that risk (e.g. show-file for the default-track-creator, §Risks #1; standalone Resource for `EffectFixtureBinding`, §Risks #3); (b) if no fallback, **stub it minimally so the tree still builds**, mark `// TODO(v0.2-blocked): <ref>`, and move to the next task. A block must never halt the run.
5. **Every commit compiles.** If a phase can't fully finish, commit the compiling subset tagged `[WIP]` with a `QUESTIONS.md` note, then start the next phase's independent work. Never commit a tree that fails `build.bat`.
6. **Ponytail every implementation choice.** Laziest option that actually works: reuse what's in the repo before writing new; framework/stdlib before custom; one line before fifty; delete before add. Mark deliberate shortcuts `// ponytail: <why> [<upgrade path>]`. **Never** simplify away: the verified NAP init-order/`start()`/serialize-list steps, input validation at trust boundaries, error handling, or the main-thread-only parameter-write rule (§Risks #2).
7. **NAP skills are the source of truth** — `nap-resource-graph`, `nap-adding-a-module-class`, `nap-build-run-verify`, `nap-validate-data-json`, `nap-sequence`, `nap-artnet-dmx`, `nap-midi`, `nap-parameters`. Validate every `objects.json` edit with `nap-validate-data-json` **before** building. Dispatch a `nap-implement` agent only for a large, fully-specified isolated class if it speeds things up — otherwise implement directly (more reliable unattended). Sub-agents must inherit these same no-pause rules.
8. **Timebox, don't spin.** If any single task exceeds ~5 serious iterations with no progress, take rule 4's fallback. Forward motion over perfection.

---

## Verification in auto mode (no human to watch the lights)

A phase is **done** when ALL hold:
- `regenerate.bat` (if files were added/removed) exits 0.
- `build.bat` exits 0 with **Napkin closed**.
- The app **launches**, the ResourceManager load log shows **zero errors** (no `Unknown object type`, `embedded pointer`, `unresolved`, `failed to load`), the process reaches its update loop, then is terminated.

**Smoke-launch:** run the built executable in the background, capture output for ~10 s or until the load log has printed, confirm it's clean and not crashed, then kill the process. If the GUI app can't be cleanly auto-terminated, accept **build-clean + load-log-clean** as the gate and note it.

**What can NOT be auto-verified** (hardware/behavioral checks from PLAN.md — DMX on the fixtures, MIDI device response, "LFO pulses a dimmer", ADSR release-linger feel, LTP revert order): log each as a `MANUAL-VERIFY` item in `QUESTIONS.md` and proceed. **Do not block on them.**

---

## Git protocol

- Branch: **`refactor/v0.2`** (created at setup; do not touch `main`).
- **Commit after each phase** and at safe sub-steps. Message: `v0.2 phase N: <summary>`.
- **Stage explicit paths** (`git add src/ module/ data/ app.json module/module.json ...`). **Never `git add -A`** — that keeps `QUESTIONS.md` and stray build artifacts out.
- **Push after each phase**: `git push` (the first push sets upstream: `git push -u origin refactor/v0.2`). Pushing per phase means progress survives if the machine dies.
- **`QUESTIONS.md` is git-ignored during the run** and is force-added + committed + pushed **only in the Final step**, after all phases (per the operator's instruction that flags ship last).
- **Do not merge to `main` and do not open a PR** — those are post-run human decisions.

---

## Phases

Design detail is in PLAN.md §§1–11 — do **not** restate it, follow it. Each phase ends with: **`nap-validate-data-json` (if JSON changed) → `regenerate.bat` (if files added/removed) → `build.bat` → smoke-launch → commit → push.** All new classes live in `namespace lx`.

### Phase 1 — Fixture ECS + demolition (PLAN §1, §11) — *irreducibly atomic, one commit*
- [ ] Add `channelrole.*`, `fixturechannelmapping.*`, `fixturechannelcomponent.*`, `fixturecomponent.*` (namespace `lx`); compile the classes first.
- [ ] Rewrite `data/objects.json` to the ECS rig (§1 sketch): 3 fixture Entities × (1 `lx::FixtureComponent` + 22 `lx::FixtureChannelComponent`), wired to `Universe0`, base params `StrobeN_*_Base`. Add the 3 entities to the Scene; remove `FixtureDriverEntity`. Keep `Window`/`Universe0`/`MidiPort`/`MidiMonitorEntity`/camera/gnomon/the 3 `ParameterGroup`s + `ParameterGUI`s.
- [ ] Strip `lxcontrolservice.*` (into `namespace lx`) to: hot-plug + MIDI subscription + log/learn accessors + fixture registry (`registerFixture`/`unregisterFixture`/`getFixtures`) + save/load stubs.
- [ ] Reduce the app to two tabs: **Fixtures** (the 3 `ParameterGUI` columns + per-channel resolved-value/claim debug) and **MIDI** (device list + live log).
- [ ] **Delete** the 13 file pairs (§11) **and** `data/user_content.json` (+ `data/presets/`) in this same commit.
- [ ] Verify: clean load log; smoke-launch OK. Commit `v0.2 phase 1: fixture ECS + demolition`; push.

### Phase 2 — Effects + Parameters + Modulators + custom Output/Adapter (PLAN §2, §3, §4, §9)
- [ ] Add `effectparameter.*` (§2 final: raw floats + `mCurrentValues`, **no `nap::Parameter` composition**).
- [ ] Add the three custom sequence classes: `modulatortrack.*` (bare `nap::SequenceTrack`), `modulatoroutput.*` (`nap::SequencePlayerOutput`, sink `ParameterFloat` + `mModulator` back-ptr + main-thread `update()` flush + `ObjectCreator<lx::ModulatorOutput, nap::SequenceService>`), `modulatoradapter.*` (`nap::SequencePlayerAdapter`, mutex-guarded `tick`→store, `flush`→param on main thread; no RTTI).
- [ ] Service wiring: add `RTTI_OF(nap::SequenceService)` to `getDependentServices()`; add `registerObjectCreators()`; in `init()` call `registerAdapterFactoryFunc(RTTI_OF(lx::ModulatorTrack), …)` + `registerDefaultTrackCreatorForOutput(RTTI_OF(lx::ModulatorOutput), …)`.
- [ ] **CHECKPOINT (do this before building the rest):** build with one `lx::ModulatorOutput`-backed player; log `player->getSequenceConst().mTracks` right after `init()` to confirm the `ModulatorTrack` was auto-created (PLAN §Risks #1). If empty → take the show-file fallback and log it. Only proceed once confirmed.
- [ ] Add `modulator.*` base, `adsrmodulator.*`, `lfomodulator.*`, `stepmodulator.*`, `curvemodulator.*` (Curve uses the shipped `SequencePlayerCurveOutput` + `SequenceEditor` + `SequenceEditorGUI`), `effect.*`.
- [ ] Service: effect/param CRUD, `addModulator` (per-modulator player+clock+output build, §9), effect `update()` blend loop, an audition path, `save()`/`rebuildFromLoadedContent()` per §9 persistence.
- [ ] App: **Effects** tab — param add/edit (hand-rolled `SliderFloat`/`Checkbox`/`ColorEdit3` via `setComponentValue`), modulator add + per-type `drawGui()` (PlotLines for ADSR/LFO/Step; Timeline toggle for Curve), clock combo, audition toggle.
- [ ] Verify: build clean; smoke-launch clean; create-one-of-each round-trips through save/reload (log a `MANUAL-VERIFY` for DMX behavior). Commit `v0.2 phase 2: effects + modulators + custom output/adapter`; push.

### Phase 3 — Triggers + full LTP (PLAN §5, §9)
- [ ] Add `trigger.*` (`lx::EffectFixtureBinding` struct + `Trigger`/`ControllerTrigger`/`EnterTrigger`/`ExitTrigger`).
- [ ] Service: `Activation` list, `fireTrigger`/`stopTrigger`, claim push/reap, release-linger, **delete-path guard** (§9).
- [ ] App: **Triggers** tab (create form, bindings table from the fixture registry, Fire/Stop, active/releasing indicator).
- [ ] Verify: build/smoke clean; **inspect the saved JSON** to confirm `EffectFixtureBinding` (first vector-of-struct) round-trips — if it doesn't, take the standalone-Resource fallback (§Risks #3) and log it. Log LTP-order/revert as `MANUAL-VERIFY`. Commit `v0.2 phase 3: triggers + LTP`; push.

### Phase 4 — Controllers + MIDI bindings + Learn (PLAN §6, §7, §9)
- [ ] Add `controller.*` (`lx::Controller`, `EControllerMode`), `midibinding.*` (`lx::MidiBinding`, filter block ported from `midimapping.*`).
- [ ] Service: `onMidiEvent` dispatch (arm unconditionally until Phase 5), CRUD + persistence.
- [ ] App: **MIDI** tab rebuild — Controllers list + per-controller Learn (message type+number only) + bindings.
- [ ] Verify: build/smoke clean; persistence round-trips. Log device-response checks as `MANUAL-VERIFY`. Commit `v0.2 phase 4: controllers + midi bindings`; push.

### Phase 5 — Programs (PLAN §8, §9)
- [ ] Add `program.*` (`lx::Program`).
- [ ] Service: `loadProgram`/`unloadProgram`, Enter/Exit firing, **program-scoped controller arming** (only fire if the trigger is in the active program), final `save`/`rebuild`.
- [ ] App: **Programs** tab + active-program banner on every tab.
- [ ] Verify: build/smoke clean; full content save/reload round-trip. Log show-playback checks as `MANUAL-VERIFY`. Commit `v0.2 phase 5: programs`; push.

---

## Final step (after Phase 5 passes)

1. Consolidate and tidy `QUESTIONS.md` (Decisions / Blocked / Manual-verify sections).
2. Ship it: `git add -f QUESTIONS.md && git commit -m "v0.2: deferred questions, flags, and manual-verify items" && git push`.
3. Print a short end-of-run summary (phases completed, any `[WIP]`/`BLOCKED`, count of manual-verify items). **Stop.** Do not merge to `main`, do not open a PR — leave that for the operator's return.

If the run is interrupted mid-phase and restarted: read the last commit + `QUESTIONS.md`, resume at the first unchecked box.
