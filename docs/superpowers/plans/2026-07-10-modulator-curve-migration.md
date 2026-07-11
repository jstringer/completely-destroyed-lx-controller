# Modulator → Curve-Backed Engine Migration — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. This is a NAP C++ app: there is **no unit-test harness** — every task is verified by **build + run + observe** per the `nap-build-run-verify` skill. "Test" below means a concrete runtime observation, not a test framework.

**Goal:** Replace the modulator subsystem's fake/segment-less `SequencePlayer` + procedural `evaluate()` + hand-rolled monotonic clock with a real napsequence **curve-backed engine**: each modulator authors a float curve that the stock `SequencePlayerCurveAdapter` samples using the player's own time, driven by the player transport — while keeping the modulator integration spirit (Target/Component, Min/Max, Blend, trigger/gate → release-linger → `reapClaims`).

**Architecture:** Each modulator owns a stock graph — `ParameterFloat` sink ← `SequencePlayerCurveOutput` ← `SequenceTrackCurveFloat` (authored at runtime via a `SequenceEditor` + `SequenceControllerCurve`) ← `SequencePlayer` + `SequencePlayerStandardClock`. A per-subtype `generateCurve()` writes the shape's keyframes; per-subtype `onTrigger()/onStop()/update()` map gate/trigger events onto the player transport (`setIsPlaying`/`setPlayerTime`/`setIsPaused`/`setIsLooping`/`setPlaybackSpeed`). `Modulator::value()` still reads the sink and maps to `[Min,Max]` — unchanged, so `Effect::update` blending and persistence are barely touched.

**Tech Stack:** NAP 0.8.0, C++, `napsequence`/`napsequencegui`/`napparameter`/`napartnet`, ImGui. Build via `regenerate.bat`/`build.bat`; run the produced exe and observe the Effects tab.

## Global Constraints

- **Never hand-edit** `CMakeLists.txt`, `cached_*_json.cmake`, `msvc64/` — regenerate instead (`nap-build-run-verify`).
- **Run `regenerate.bat` after adding or removing any `.cpp` in `module/src/`** — not just after manifest changes. A missed regenerate silently drops the file from the build (symptom: `Unknown object type` at load, or stale behavior).
- **Close Napkin** before building or `build.bat` fails with `LNK1104` on `naplxcontrol.dll`.
- All new module classes live in namespace `lx`, register RTTI in their `.cpp`, and are `NAPAPI` if referenced outside the module.
- `"Type"` is reserved in NAP JSON — never a custom property name.
- Runtime-constructed resources need every init-order step called by hand, leaves-first: `init(errorState)` on each, and `start()` for `Device`s (the `SequencePlayer`).
- Commit after every task with a `v0.2` prefix, no `Co-Authored-By`, no AI references.
- Accepted ceilings (from the design spec): sine/gaussian are bezier **approximations**; ADSR release always starts from the sustain level; curve regeneration is done on the main thread only (safe because the `StandardClock` ticks on the main thread).

---

## File Structure

**New files (`module/src/`):**
- `admodulator.h` / `admodulator.cpp` — the new AD (attack-decay) shape type.

**Rewritten in place:**
- `modulator.h` / `modulator.cpp` — base stripped of procedural clock; gains `generateCurve()`, per-frame `update()`, curve-graph runtime handles.
- `adsrmodulator.*`, `lfomodulator.*`, `stepmodulator.*` — procedural `evaluate()` replaced by `generateCurve()` + transport overrides.
- `lxcontrolservice.cpp` — `buildModulatorGraph()` rewritten to build the stock curve graph; the `authorFloatCurve()` helper added; the two `SequenceService` factory registrations + the `ModulatorOutput` object-creator removed; `addModulator` gains AD.
- `effect.cpp` — `Effect::update` calls `update(dt)` instead of `advance(dt)`.
- `src/lxcontrolapp.cpp` — Effects-tab GUI: add "Add AD", Blend + Mode combos, keep the value meter, add a curve plot.

**Deleted (final task):**
- `modulatortrack.*`, `modulatoradapter.*`, `modulatoroutput.*`.

**Temporary scaffold:** `Modulator::usesCurve()` (default false) — lets migrated subtypes run the curve path while un-migrated ones keep the old procedural path, so the app builds/runs between tasks. Removed in the final task once all subtypes are curve-based.

---

## Task 1: Spike — prove runtime curve authoring drives a parameter (THROWAWAY)

**Goal:** De-risk the entire plan. Prove that a `SequencePlayer` + stock `SequencePlayerCurveOutput` + a `SequenceTrackCurveFloat` authored at runtime via `SequenceEditor`/`SequenceControllerCurve` actually animates a `ParameterFloat` off the player's own time — and pin down the exact controller call sequence (including a flat/stepped segment for square/step shapes). The output is knowledge + a proven `authorFloatCurve()` recipe, not shipped structure.

**Files:**
- Modify (temporary): `module/src/lxcontrolservice.cpp` — add a `spikeCurveTest()` method called once from `lxcontrolService::init()` after the `SequenceService` is available; guarded so it's trivially removable.

**Interfaces:**
- Produces (knowledge for later tasks): the working body of
  `void authorFloatCurve(nap::SequenceEditor& editor, const std::string& trackID, const std::vector<Key>& keys, double duration)`
  where `struct Key { double time; float value; nap::math::ECurveInterp interp; };` — `time` absolute seconds, `value` 0..1.

- [ ] **Step 1: Write the spike**

Add to `lxcontrolservice.cpp` (include `<sequenceeditor.h>`, `<sequencecontrollercurve.h>`, `<sequenceplayercurveoutput.h>`, `<parameternumeric.h>`):

```cpp
void lxcontrolService::spikeCurveTest()
{
    using namespace nap;
    utility::ErrorState err;

    auto sink = mResourceManager->createObject<ParameterFloat>();
    sink->mID = "SPIKE_Sink"; sink->mMinimum = 0.0f; sink->mMaximum = 1.0f; sink->mValue = 0.0f;
    if (!sink->init(err)) { Logger::error("spike sink: %s", err.toString().c_str()); return; }

    auto output = mResourceManager->createObject<SequencePlayerCurveOutput>();
    output->mID = "SPIKE_Output"; output->mParameter = sink; output->mUseMainThread = true;
    if (!output->init(err)) { Logger::error("spike output: %s", err.toString().c_str()); return; }

    auto clock = mResourceManager->createObject<SequencePlayerStandardClock>();
    clock->mID = "SPIKE_Clock";
    if (!clock->init(err)) { Logger::error("spike clock: %s", err.toString().c_str()); return; }

    auto player = mResourceManager->createObject<SequencePlayer>();
    player->mID = "SPIKE_Player";
    player->mSequenceFileName = "SPIKE_nonexistent.json";
    player->mCreateEmptySequenceOnLoadFail = true;
    player->mClock = clock;
    player->mOutputs.emplace_back(rtti::ObjectPtr<SequencePlayerOutput>(output.get()));
    if (!player->init(err) || !player->start(err)) { Logger::error("spike player: %s", err.toString().c_str()); return; }

    auto editor = mResourceManager->createObject<SequenceEditor>();
    editor->mID = "SPIKE_Editor"; editor->mSequencePlayer = player;
    if (!editor->init(err)) { Logger::error("spike editor: %s", err.toString().c_str()); return; }

    auto& ctrl = editor->getController<SequenceControllerCurve>();
    ctrl.addNewCurveTrack<float>();
    // Discover the new track's id (last track added), main thread only.
    const std::string trackID = player->getSequenceConst().mTracks.back()->mID;
    ctrl.assignNewOutputID(trackID, output->mID);
    ctrl.changeMinMaxCurveTrack<float>(trackID, 0.0f, 1.0f);

    // --- SPIKE OBJECTIVE: author a 1s ramp 0->1, then a stepped square, via the controller,
    //     and record the exact calls that work into authorFloatCurve(). Start from the default
    //     segment created by addNewCurveTrack and manipulate it with:
    //       ctrl.insertSegment(trackID, time), ctrl.segmentDurationChange(...),
    //       ctrl.changeCurveSegmentValue(trackID, segID, value, 0, BEGIN|END),
    //       ctrl.insertCurvePoint(trackID, segID, pos0..1, 0),
    //       ctrl.changeCurvePoint(trackID, segID, pointIdx, 0, time0..1, value),
    //       ctrl.changeCurveType(trackID, segID, math::ECurveInterp::Linear|Bezier, 0)
    editor->changeSequenceDuration(1.0);
    player->setIsLooping(true); player->setPlayerTime(0.0); player->setIsPlaying(true);

    mSpikeSink = sink.get(); // ParameterFloat* member, log in update()
}
```

In `lxcontrolService::update()` (already runs every frame), temporarily add:
```cpp
if (mSpikeSink != nullptr)
    Logger::info("SPIKE sink = %.3f", mSpikeSink->mValue);
```

- [ ] **Step 2: Regenerate + build**

Run: `regenerate.bat` (no new files, but safe) then `build.bat`.
Expected: build succeeds, no `LNK1104` (close Napkin first).

- [ ] **Step 3: Run and observe**

Run the produced exe (`nap-build-run-verify` for the exact path).
Expected: the log shows `SPIKE sink =` **ramping 0.000 → 1.000 and repeating** every second. This proves the player animates a parameter off its own time via a runtime-authored curve.

- [ ] **Step 4: Record the recipe**

In the plan's margin / a scratch note, capture the exact working call sequence for (a) a 2-point linear ramp and (b) a stepped square (two flat segments, or the epsilon-step fallback if napsequence can't do instantaneous steps). This becomes `authorFloatCurve()` in Task 3.
Acceptance: you can author both a smooth ramp and a hard step; note which (if either) required the epsilon-step fallback → that's the documented square/pulse ceiling.

- [ ] **Step 5: Remove the spike, commit the knowledge**

Delete `spikeCurveTest()`, its call, `mSpikeSink`, and the temporary log. Commit the *plan note* only (no code):
```bash
git commit --allow-empty -m "v0.2 spike: proven runtime curve authoring drives a ParameterFloat (recipe captured)"
```

---

## Task 2: `authorFloatCurve()` helper + `Key` type (shipped)

**Goal:** Turn the spike's proven recipe into a permanent, reusable helper so shape generators never touch the controller directly.

**Files:**
- Modify: `module/src/lxcontrolservice.h` — declare the helper + `Key` struct.
- Modify: `module/src/lxcontrolservice.cpp` — implement it from the Task-1 recipe.

**Interfaces:**
- Produces:
  `struct lx::Key { double mTime; float mValue; nap::math::ECurveInterp mInterp; };`
  `void lxcontrolService::authorFloatCurve(nap::SequenceEditor& editor, const std::string& trackID, const std::vector<lx::Key>& keys, double duration);`
  Contract: clears the track to a single segment of `duration` seconds, then lays the keys down at their absolute times with the given interpolation, values already 0..1. Last key's time should equal `duration`.

- [ ] **Step 1: Declare** the `Key` struct and method in `lxcontrolservice.h` (near the other helpers).
- [ ] **Step 2: Implement** `authorFloatCurve()` in `lxcontrolservice.cpp` using the exact sequence proven in Task 1.
- [ ] **Step 3: Build** (`build.bat`). Expected: succeeds; helper unused yet, so no behavior change.
- [ ] **Step 4: Commit**
```bash
git add module/src/lxcontrolservice.h module/src/lxcontrolservice.cpp
git commit -m "v0.2: add authorFloatCurve() runtime curve helper"
```

---

## Task 3: Base `Modulator` — strip procedural clock, add curve hooks + `usesCurve()` shim

**Goal:** Reshape the base so a subtype can be curve-driven, without breaking the still-procedural subtypes (they keep working via `usesCurve()==false`).

**Files:**
- Modify: `module/src/modulator.h`, `module/src/modulator.cpp`

**Interfaces:**
- Produces (base API later tasks rely on):
  - `virtual bool usesCurve() const { return false; }` (shim)
  - `virtual void generateCurve(lxcontrolService& svc, nap::SequenceEditor& editor, const std::string& trackID) {}` — subtype authors its shape; sets `mDuration` (+ `mSustainTime` for envelopes).
  - `virtual void update(double deltaTime) {}` — per-frame transport housekeeping (replaces `advance`).
  - Runtime handles (non-serialized): `nap::SequencePlayer* mPlayer; nap::ParameterFloat* mSink; nap::SequenceEditor* mEditor; std::string mTrackID; double mDuration = 0.0; double mSustainTime = 0.0; bool mReleased = false;`
  - `value()` unchanged. `onTrigger()/onStop()/isFinished()` remain virtual.

- [ ] **Step 1:** In `modulator.h`, keep properties `Name/Target/TargetComponent/Min/Max/Blend`; **delete** `mClock`, `mClockFrequency`, `EModulatorClock`, and the procedural fields `mHeld/mReleasing/mElapsed/mReleaseElapsed/mReleaseFrom`. Add the runtime handles + `mReleased` above. Declare `usesCurve`, `generateCurve`, `update`. Keep `onTrigger/onStop/isFinished/value` signatures.
- [ ] **Step 2:** In `modulator.cpp`, remove the `EModulatorClock` RTTI enum and the `Clock`/`ClockFrequency` properties. Reduce `onTrigger()/onStop()` to base no-ops that flip `mReleased` (`onTrigger`: `mReleased=false`; `onStop`: `mReleased=true`). Delete `advance()` and `evaluate()`; keep `value()` (reads `mSink`). Provide a base `update()` no-op and `isFinished()` returning `mReleased` (subtypes refine).
- [ ] **Step 3:** This will not build until subtypes stop calling the removed members — so this task’s build happens **after** Steps in Task 4/5/6 land the subtype fixes. To keep Task 3 independently green, temporarily leave `evaluate()`/`advance()` as deprecated inline stubs in the header (`float evaluate() const { return 0.0f; }`, `void advance(double){}`) marked `// ponytail: temporary shim, removed in Task 8`. Now it builds.
- [ ] **Step 4: Build.** Expected: succeeds; runtime unchanged (subtypes still procedural via stubs → they now output 0; acceptable mid-migration).
- [ ] **Step 5: Commit**
```bash
git add module/src/modulator.h module/src/modulator.cpp
git commit -m "v0.2: base Modulator gains curve hooks + usesCurve shim (procedural stubs temporary)"
```

---

## Task 4: `buildModulatorGraph()` — build the stock curve graph

**Goal:** Replace the fake segment-less player with a real curve graph. Branch on `usesCurve()`: curve subtypes get the new graph; others keep a minimal legacy path until migrated.

**Files:**
- Modify: `module/src/lxcontrolservice.cpp` (`buildModulatorGraph`, `:244-299`), `lxcontrolservice.h` (`ModulatorEntry` gains `mTrackID`).

**Interfaces:**
- Consumes: `authorFloatCurve` (Task 2), `Modulator::generateCurve/usesCurve/mDuration` (Task 3).
- Produces: after this task, a `usesCurve()` modulator has `mPlayer/mSink/mEditor/mTrackID` wired to a stock `SequencePlayerCurveOutput` graph, built stopped (plays on trigger).

- [ ] **Step 1:** Rewrite the graph build: sink `ParameterFloat` (0..1) → `SequencePlayerCurveOutput` (`mParameter=sink`, `mUseMainThread=true`) → `SequencePlayerStandardClock` → `SequencePlayer` (`mOutputs=[curveOutput]`, empty-on-fail) `init`+`start` → `SequenceEditor` (`init`). Then `editor->getController<SequenceControllerCurve>().addNewCurveTrack<float>()`, capture `trackID = player->getSequenceConst().mTracks.back()->mID`, `assignNewOutputID(trackID, output->mID)`, `changeMinMaxCurveTrack<float>(trackID,0,1)`. Call `mod->generateCurve(*this, *editor, trackID)` then `editor->changeSequenceDuration(mod->mDuration)`. Store handles. **Do not** auto-play: `player->setPlayerTime(0.0); player->setIsPlaying(false);`.
- [ ] **Step 2:** Guard with `if (mod->usesCurve()) { ...new... } else { ...keep the old duration=3600 loop heartbeat... }` so un-migrated subtypes still function.
- [ ] **Step 3: Build.** Expected: succeeds. No `usesCurve()` subtype exists yet → all still legacy path; behavior unchanged.
- [ ] **Step 4: Commit**
```bash
git add module/src/lxcontrolservice.cpp module/src/lxcontrolservice.h
git commit -m "v0.2: buildModulatorGraph builds a stock curve graph for usesCurve modulators"
```

---

## Task 5: Migrate `AdsrModulator` to curves (first end-to-end shape)

**Goal:** First shape fully on the new engine — proves the whole pipeline through the GUI and DMX.

**Files:**
- Modify: `module/src/adsrmodulator.h`, `module/src/adsrmodulator.cpp`
- Modify: `module/src/effect.cpp` (`advance` → `update`)

**Interfaces:**
- Consumes: Task 3 base hooks, Task 4 graph.
- Curve geometry (linear, exact): keys `(0,0)`, `(A,1)`, `(A+D,S)`, `(A+D+R,0)`; `mDuration=A+D+R`; `mSustainTime=A+D`.

- [ ] **Step 1:** `adsrmodulator.h`: keep `A/D/S/R/Loop`. Override `usesCurve()→true`, `generateCurve`, `onTrigger`, `onStop`, `update`, `isFinished`. Remove `evaluate`.
- [ ] **Step 2:** `generateCurve`: build the 4 keys above (all `ECurveInterp::Linear`), set `mDuration`/`mSustainTime`, call `svc.authorFloatCurve(editor, trackID, keys, mDuration)`.
- [ ] **Step 3:** Transport: `onTrigger` → `mReleased=false; mPlayer->setPlayerTime(0); mPlayer->setIsPaused(false); mPlayer->setIsLooping(mLoop); mPlayer->setIsPlaying(true);`. `update` → if `!mLoop && !mReleased && mPlayer->getIsPlaying() && mPlayer->getPlayerTime() >= mSustainTime` then `mPlayer->setIsPaused(true)` (hold sustain). `onStop` → `mReleased=true; mPlayer->setPlayerTime(mSustainTime); mPlayer->setIsPaused(false);` (release; ceiling: always from sustain). `isFinished` → `mReleased && mPlayer->getPlayerTime() >= mDuration` (and stop the player there in `update`: if finished, `setIsPlaying(false)`).
- [ ] **Step 4:** `effect.cpp:32`: replace `modulator->advance(deltaTime);` with `modulator->update(deltaTime);`.
- [ ] **Step 5: Build.** Expected: succeeds.
- [ ] **Step 6: Run and observe.** Add an ADSR to an effect, bind/trigger it (GUI Trigger button). Expected: the value meter rises over Attack, falls to Sustain, **holds** while held, and rings out over Release on Stop; a fixture channel driven by it follows. This is the end-to-end proof.
- [ ] **Step 7: Commit**
```bash
git add module/src/adsrmodulator.h module/src/adsrmodulator.cpp module/src/effect.cpp
git commit -m "v0.2: ADSR modulator on curve-backed engine (sustain-by-pause), Effect::update drives update()"
```

---

## Task 6: Add the `AdModulator` type

**Goal:** New AD shape with OneShot / LoopWhileSustained modes.

**Files:**
- Create: `module/src/admodulator.h`, `module/src/admodulator.cpp`
- Modify: `module/src/lxcontrolservice.cpp` (`addModulator` awareness is automatic via `createObject(type)`; add a GUI button in Task 9).

**Interfaces:**
- Curve geometry (linear): keys `(0,0)`, `(A,1)`, `(A+D,0)`; `mDuration=A+D`. Enum `EAdMode { OneShot, LoopWhileSustained }`.

- [ ] **Step 1:** Create the class deriving `Modulator`, properties `Attack`, `Decay`, `Mode`; RTTI enum + class in the `.cpp`. `usesCurve()→true`.
- [ ] **Step 2:** `generateCurve` writes the 3 keys. `onTrigger` → OneShot: `setPlayerTime(0); setIsLooping(false); setIsPlaying(true)`; LoopWhileSustained: `setPlayerTime(0); setIsLooping(true); setIsPlaying(true)`. `onStop` → LoopWhileSustained: `setIsLooping(false)` (finish current cycle; ceiling). `update` → stop at end when not looping (`getPlayerTime()>=mDuration → setIsPlaying(false)`). `isFinished` → `!mPlayer->getIsPlaying()`.
- [ ] **Step 3: `regenerate.bat`** (new `.cpp`!) then **`build.bat`**. Expected: succeeds; if you skip regenerate, load fails with `Unknown object type lx::AdModulator`.
- [ ] **Step 4: Commit**
```bash
git add module/src/admodulator.h module/src/admodulator.cpp
git commit -m "v0.2: add AD modulator (OneShot / LoopWhileSustained)"
```

---

## Task 7: Migrate `LfoModulator` and `StepModulator` to curves

**Goal:** Move the remaining shapes onto the engine; retire procedural `evaluate()` everywhere.

**Files:**
- Modify: `lfomodulator.h/.cpp`, `stepmodulator.h/.cpp`

**Interfaces (curve geometry):**
- **LFO** — a **1-second one-period** curve; frequency = `setPlaybackSpeed(mFrequency)`; `mDuration=1.0`. Modes `ELfoMode { Loop, OneShot, LoopRetrigger }`. Shapes:
  - Ramp: linear `(0,0),(1,1)`. Triangle: linear `(0,0),(0.5,1),(1,0)`.
  - Square: two flat segments (exact) — `(0,1),(0.5,1),(0.5,0),(1,0)` via the helper’s step support (or epsilon-step fallback from Task 1). Pulse: same with the split at `mPulseWidth`.
  - Sine: bezier, keys `(0,0.5),(0.25,1),(0.5,0.5),(0.75,0),(1,0.5)` (approximation ceiling). Gaussian: bezier, 5–7 keys sampling `exp(-((p-0.5)*width)^2)` (approximation ceiling).
  - PhaseOffset → initial `setPlayerTime(mPhaseOffset)`.
- **Step** — N flat segments (no glide) or linear (glide); `mDuration = mSteps.size()/mRate`; Clock mode loops per `mLoop`. Trigger-advance mode: `onTrigger` → `setPlayerTime(stepIndex/mRate)` (the strained case; ceiling: trigger-advance is a seek, glide ignored across triggers).

- [ ] **Step 1:** LFO: replace `evaluate()` with `generateCurve()` (emit the per-shape keys above), add `ELfoMode`, override `onTrigger`/`onStop`/`update`/`usesCurve→true`. Set `mPlayer->setPlaybackSpeed(mFrequency)` in `generateCurve`/`onTrigger`.
- [ ] **Step 2:** Step: replace `evaluate()` with `generateCurve()` (flat/linear segments), transport per mode.
- [ ] **Step 3:** Remove the temporary `evaluate()/advance()` stubs from `modulator.h` (added in Task 3) — nothing calls them now.
- [ ] **Step 4: Build.** Expected: succeeds.
- [ ] **Step 5: Run and observe.** Trigger an LFO (watch the meter oscillate at the set Hz; change Frequency → rate changes with **no** regen), a Square (hard edges), a Step. Expected: all animate correctly.
- [ ] **Step 6: Commit**
```bash
git add module/src/lfomodulator.h module/src/lfomodulator.cpp module/src/stepmodulator.h module/src/stepmodulator.cpp module/src/modulator.h
git commit -m "v0.2: LFO + Step modulators on curve engine; drop procedural evaluate stubs"
```

---

## Task 8: Delete the custom track/adapter/output + factory registrations

**Goal:** Remove the now-dead napsequence scaffolding and the legacy branch — the engine is fully stock-curve-based.

**Files:**
- Delete: `modulatortrack.*`, `modulatoradapter.*`, `modulatoroutput.*`
- Modify: `lxcontrolservice.cpp` — remove the `ModulatorOutputObjectCreator` registration (`:36-42`), the `registerAdapterFactoryFunc`/`registerDefaultTrackCreatorForOutput` calls (`:55-68`), the legacy `else` branch in `buildModulatorGraph`, and the now-unused includes. Remove `mClock`/heartbeat remnants and `SequenceEditor`-duration-only usage.

- [ ] **Step 1:** Delete the six files and all references/includes to `ModulatorTrack/Adapter/Output`.
- [ ] **Step 2:** Remove the two factory registrations + object-creator + the `usesCurve()` legacy branch (all subtypes are curve now, so `usesCurve()` can also be deleted, or kept returning true — delete it for cleanliness).
- [ ] **Step 3: `regenerate.bat`** (files removed!) then **`build.bat`**. Expected: succeeds with no unresolved symbols.
- [ ] **Step 4: Run and observe.** Reload existing `user_content.json`; every modulator rebuilds and animates. Expected: no `Unknown object type`, no dangling-reference load error.
- [ ] **Step 5: Commit**
```bash
git add -A
git commit -m "v0.2: delete custom modulator track/adapter/output + factory registrations (fully stock curve engine)"
```

---

## Task 9: GUI — AD button, Blend + Mode combos, curve plot

**Goal:** Expose the new type + previously-unauthorable options, and show the shape.

**Files:**
- Modify: `src/lxcontrolapp.cpp` (Effects tab, `:302-353`)

- [ ] **Step 1:** Add an "Add AD" button next to Add ADSR/LFO/Step (calls `addModulator(effect, RTTI_OF(lx::AdModulator), target)`).
- [ ] **Step 2:** Add a `Blend` combo (Replace/Multiply/Add) and per-subtype `Mode` combo (ADSR Loop toggle already exists; AD/LFO modes) to each modulator row.
- [ ] **Step 3:** Replace/augment the `ProgressBar` meter with an `ImGui::PlotLines` sampling the modulator's curve — sample `mSink->mValue` into a ring buffer each frame (simplest, shows live output), or sample the track's `getValue` across the duration for a static shape preview.
- [ ] **Step 4: Build + run.** Expected: AD is addable; Blend/Mode editable and take effect; the plot tracks the live value.
- [ ] **Step 5: Commit**
```bash
git add src/lxcontrolapp.cpp
git commit -m "v0.2: modulator GUI - AD button, Blend/Mode combos, live curve plot"
```

---

## Task 10: Persistence round-trip verification

**Goal:** Confirm the reduced serialization set still round-trips (only the `Modulator` resource is saved; the curve graph is rebuilt on load).

**Files:** none (verification only); fix `save()`/`rebuildFromLoadedContent` only if a gap surfaces.

- [ ] **Step 1:** Author one of each type (ADSR/AD/LFO/Step) with non-default params, Blend, Mode. Trigger to confirm behavior.
- [ ] **Step 2:** Restart the app. Expected: all four reload with identical params/behavior; no load error. `AdModulator` and new enum props must appear in `user_content.json`.
- [ ] **Step 3:** If a subtype fails to reload, confirm its new properties are RTTI-registered and that `save()` still lists `me.mModulator` (it should need no change). Fix, rebuild, re-verify.
- [ ] **Step 4: Commit** any fixes:
```bash
git add -A
git commit -m "v0.2: verify modulator persistence round-trip on curve engine"
```

---

## Self-Review

**Spec coverage** (`docs/superpowers/specs/2026-07-10-parametric-sequence-shapes-design.md` + the migration decision):
- Curve-backed engine, player time as timebase → Tasks 4–7. ✅
- ADSR sustain-by-pause → Task 5. ✅
- AD OneShot / LoopWhileSustained → Task 6. ✅
- LFO Loop / OneShot / LoopRetrigger, freq via playback speed → Task 7. ✅
- Sine/gaussian approximation, square/pulse exact segments → Task 7 + Task 1 step support. ✅
- Keep integration spirit (Target/Component/Min/Max/Blend, trigger→release-linger) → `value()`/`Effect::update` preserved (Tasks 3, 5); Blend GUI (Task 9). ✅
- Delete framework-fighting scaffolding (monotonic clock, heartbeat, custom track/adapter/output) → Tasks 3, 8. ✅
- StandardClock-only, main-thread regen safety → Tasks 4, Global Constraints. ✅

**Placeholder scan:** the only deferred specifics are the exact `SequenceControllerCurve` call sequence inside `authorFloatCurve()`, which is **intentionally** pinned by the Task-1 spike before any dependent task runs — not a placeholder but a de-risking step with explicit acceptance criteria.

**Type consistency:** `Key`/`authorFloatCurve` signature is defined once (Task 2) and consumed unchanged (Tasks 5–7); `usesCurve`/`generateCurve`/`update`/`mDuration`/`mSustainTime`/`mReleased` defined in Task 3 and used consistently after.

**Ordering safety:** the `usesCurve()` shim + temporary `evaluate()/advance()` stubs keep every task building and running; both are removed in Tasks 7–8 once no caller remains.
