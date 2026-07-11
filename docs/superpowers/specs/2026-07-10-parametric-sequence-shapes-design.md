# Parametric Sequence Shapes (ADSR / AD / LFO) — Design

**Date:** 2026-07-10
**Status:** Design / theory reference. **Adopted as the target for the modulator→curve migration** (see `../plans/2026-07-10-modulator-curve-migration.md`).

> **Migration decision (2026-07-10):** the existing `lx` modulator subsystem (procedural `evaluate()` on a hand-rolled monotonic clock, a segment-less `ModulatorTrack`, and a `SequencePlayer` looped only as a heartbeat) is being replaced by the **curve-backed A+B merge** below: shapes generate a real `SequenceTrackCurveFloat` sampled by the stock `SequencePlayerCurveAdapter` off the player's own time, with the player transport driving gate/loop/one-shot/retrigger. The modulator *integration spirit* (Target/Component, Min/Max, Blend, trigger→release-linger→`reapClaims`) is kept as a thin layer; `Modulator::value()` and persistence are preserved. Sine/gaussian approximation accepted. This supersedes the "standalone" framing — the shapes are the modulator engine, not a separate feature.
**Scope:** How to build new "types of Sequence" — parametric envelope/LFO shapes — on top of NAP's `napsequence` ecosystem, starting with a proof-of-concept (Approach A) engineered so it can be promoted to first-class track types (Approach B) without a rewrite.

All source references are to the shipped headers under
`system_modules/napsequence/include/` (the SDK ships headers only; `.cpp` bodies
live in the prebuilt lib, so behavior is grounded in declarations + doc-comments,
and places where the header does **not** specify behavior are flagged as such).

---

## 1. Purpose

Provide reusable, GUI-editable **preset shapes** — an ADSR envelope, an AD envelope,
and an LFO — that generate a value over time and drive a `nap::Parameter`. Each shape
is, under the hood, a single-track `Sequence`. Multi-track composition is explicitly
out of scope for now.

The shapes must be:
- **Selectable** between a small set of named types.
- **Knob-editable** (A/D/S/R times, LFO shape/rate/depth) with a live plot preview.
- **Event-driven** via a *gate* (sustained on/off) and a *trigger* (momentary), with
  per-type play modes (one-shot, loop, loop-while-sustained, retrigger).

---

## 2. Key realization: these are curves the SDK already plays

An ADSR, an AD, and an LFO are each just **one float curve over time**. The napsequence
module already ships the entire chain that samples a curve and drives a parameter, so the
value-generation side needs **no new track type, adapter, or output**:

- `SequenceTrackCurveFloat` — a float track with `mMinimum`/`mMaximum`
  (`sequencetrackcurve.h:44-52`).
- Its segments are backed by `math::FCurve<float,float>` with per-curve **linear or
  bezier** interpolation and `getValue(0..1)` sampling
  (`sequencetracksegmentcurve.h:40-41,65`).
- `SequencePlayerCurveAdapter` samples that curve on the player thread and pushes it into
  a `Parameter`, thread-safely (`sequenceplayercurveadapter.h:79-105`).
- The player already exposes the transport we need: `setIsLooping`
  (`sequenceplayer.h:97`), `setIsPaused` + `setPlayerTime` (`:91,:103`), `setIsPlaying`
  (`:85`), and `postTick` firing on the player thread (`:202`).

What is genuinely **new** is only:
1. A **shape generator** — writes curve points from knob values.
2. A **transport controller** — maps gate/trigger events onto transport calls.
3. A **knob + read-only plot GUI**.
4. A small **shape registry** for selecting between types.

---

## 3. Architecture (Approach A — proof of concept)

### 3.1 Organizing principle: isolate the shape generator

The type-agnostic unit is a **pure shape generator**:

```
generate(params) -> fills a SequenceTrackSegmentCurveFloat's math::FCurve points
```

All "ADSR-ness / AD-ness / LFO-ness" lives here, free of any dependency on the player,
output, or GUI. In Approach A the generator fills a plain curve track; in Approach B the
*same function* becomes the track subclass's segment builder. This isolation is the
primary A→B migration seam (see §7).

### 3.2 Components

- **Generators** (pure, no NAP plumbing) — one per shape family (§4).
- **Knobs = parameters** — `ParameterFloat` / `ParameterEnum` grouped in a
  `ParameterGroup` (reuse `napparameter`): gives GUI, min/max clamping, and later
  MIDI-mappability for free. On a knob's `valueChanged`, regenerate the curve.
- **Transport controller** — the one genuinely new runtime behavior. Holds a reference to
  the `SequencePlayer` + the shape's timing constants (e.g. sustain time `A+D`) and its
  play mode, and translates *gate* / *trigger* events into transport calls (§5).
- **GUI** — `ParameterGUI` for the knobs + a read-only `ImGui::PlotLines` preview that
  samples the segment's `getValue(0..1)`. The freeform timeline editor is **hidden** for
  these shapes; they are knob-driven, not hand-drawn.
- **Shape registry** — a map `shape-id -> generator`. This indirection is the second A→B
  seam (in B these become distinct RTTI track types).

### 3.3 How one shape is constructed at runtime

Mirrors the runtime-construction discipline already used elsewhere (build leaves-first,
`init()` each object, `start()` the player since it is a `Device`):
`ParameterFloat` knobs → `SequencePlayerCurveOutput` (+ target `Parameter`) →
`SequencePlayerCurveAdapter` → `SequencePlayerStandardClock` → `SequencePlayer` → generate
the curve into the track segment → construct the transport controller.

---

## 4. Shape types and generator geometry

Values below are normalized to the track's `[mMinimum, mMaximum]` (default `[0,1]`).

- **ADSR** — params `A, D, S(level 0..1), R`.
  Points: `(0,0) → (A,1) → (A+D, S) → (A+D+R, 0)`.
  The **sustain is not geometry** — the curve is just A→D→R with the decay endpoint at the
  sustain level and a release from that level to 0. The *hold* is a playback behavior (§6).

- **AD** — params `A, D`.
  Points: `(0,0) → (A,1) → (A+D, 0)`.
  No sustain level; its "sustain" is a loop, not a hold (§5).

- **LFO** — params `shape ∈ {sine, triangle, saw, ramp, square}, period, depth, phase`.
  One period of the shape as curve points: **linear** points for triangle/saw/ramp/square
  (exact); **bezier tangents** for sine (approximation). Track plays looped with
  `duration = period`; `depth`/`phase` shift amplitude/offset.

---

## 5. Play-mode matrix (the "new type" surface)

Two event kinds drive everything: **gate** (on/off, sustained) and **trigger**
(momentary). Each type exposes a play mode; the transport controller maps events onto
transport calls.

| Type | Mode | Gate-on / Trigger | While held | Gate-off |
|---|---|---|---|---|
| **ADSR** | (single) | `setPlayerTime(0)`, play | pause at `A+D` — holds sustain level | jump to `A+D`, play R, stop at end |
| **AD** | OneShot | `setPlayerTime(0)`, play A→D once, stop | — | ignored (new trigger = `setPlayerTime(0)`) |
| **AD** | LoopWhileSustained | `setIsLooping(true)`, play | loops A→D | `setIsLooping(false)`, stop at cycle end |
| **LFO** | Loop | `setIsLooping(true)`, run | loops | — |
| **LFO** | OneShot | `setIsLooping(false)`, `setPlayerTime(0)`, play one period, stop | — | — |
| **LFO** | LoopRetrigger | `setIsLooping(true)`; trigger → `setPlayerTime(0)` (phase reset) | loops | — |

AD's "loop while sustained" and LFO's "retrigger while looping" share the same primitive
(loop flag + `setPlayerTime(0)` on event), so a single transport controller covers all
rows. That controller is exactly what becomes an ADSR/LFO-aware `SequencePlayerAdapter` in
Approach B (§7).

---

## 6. ADSR sustain-by-pause (detail)

Pausing freezes time but **still ticks adapters** (`sequenceplayer.h:81-91`): the curve
adapter keeps sampling the *frozen* time = sustain point, so the driven parameter holds at
the sustain level. Sequence:

1. **gate-on:** `setPlayerTime(0)`, `setIsPaused(false)`, `setIsPlaying(true)`.
2. **reach sustain:** when `getPlayerTime() ≥ A+D`, `setIsPaused(true)`. Poll in
   `App::update()` for the PoC; `postTick` (`:202`, fires on the player thread) is the
   precise option.
3. **gate-off:** `setPlayerTime(A+D)`, `setIsPaused(false)` → release segment plays from
   the sustain level to 0.
4. **end:** when `getPlayerTime() ≥ duration`, `setIsPlaying(false)`.

---

## 7. Approach B migration path (designed in, not bolted on)

Approach B promotes shapes to first-class track types. The three isolated seams from
Approach A move over largely unchanged:

- **Generator functions** → `SequenceTrackADSR/AD/LFO::generateSegments()`.
- **Transport controller** → an ADSR/LFO-aware `SequencePlayerAdapter` (or player-side
  behavior) that understands gating/looping natively.
- **Knobs** → track RTTI properties (or stay as bound parameters).
- **Plot GUI** → a `napsequencegui` track view registered for the new track type.
- **Shape registry** → distinct RTTI track types.

Approach B additionally requires the four `SequenceService` registrations that Approach A
needs **none** of: adapter factory, controller factory, controller-for-track-type mapping,
and default-track-creator-for-output (`sequenceservice.h:65-108`). Register these from an
app/module service that initializes before any `SequenceEditor`
(`sequenceservice.h:35-41`).

---

## 8. Threading & correctness

- **Use `SequencePlayerStandardClock`** (main-thread ticks, `sequenceplayerclock.h:55-58`)
  for Approach A. Then curve **regeneration and playback cannot race** — both are on the
  main thread — so no mutex handling is required. Regenerate only in response to
  main-thread events (parameter `valueChanged` from the GUI), never from the player
  thread.
- **Tripwire for later:** if anyone swaps in `SequencePlayerIndependentClock`
  (its own thread at `Frequency` Hz, `sequenceplayerclock.h:104-141`) — e.g. for sub-frame
  LFO precision driving fast DMX — then curve regeneration **must** be guarded like
  `SequenceEditor` edits (`sequenceplayer.h:272,281`; `sequenceeditor.h:45-47,195`),
  because ticks would then run off the main thread.
- Curve output stays `Use Main Thread: true` (default, `sequenceplayercurveoutput.h:33`) so
  parameter writes happen on the main thread.
- **End-of-sequence when not looping is unspecified in the shipped headers.** `setIsLooping`
  only documents that looping *restarts* at the end (`sequenceplayer.h:94-97`); it does not
  say what a non-looping player does at the end (auto-stop vs clamp vs keep counting).
  Therefore the transport controller **detects end explicitly**
  (`getPlayerTime() ≥ duration → setIsPlaying(false)`) rather than assuming auto-stop. This
  keeps OneShot and "stop at cycle end" robust regardless of the unshown implementation.

---

## 9. Known ceilings (deliberate simplifications)

- **ADSR release** always starts from the sustain level; a note-off during attack/decay
  snaps to sustain-then-release. Upgrade: capture the instantaneous value at gate-off and
  regenerate a release-from-current curve.
- **AD "stop at cycle end"** finishes the current A→D pass on gate-off rather than cutting
  immediately. Immediate-cut is a later sub-option.
- **LFO retrigger** is a hard phase reset to 0, not phase-continuous.
- **Sine LFO** is a bezier approximation, not analytically exact. Upgrade: more control
  points, or a real track type (B) with an analytic adapter.
- **Single-track only** — one shape = one single-track player. Multi-track is future work.

---

## 10. Reference index (SDK headers)

- `sequenceplayer.h` — player transport, signals, adapter management, edit mutex.
- `sequenceplayerclock.h` — StandardClock (main thread) vs IndependentClock (own thread).
- `sequenceplayercurveoutput.h` / `sequenceplayercurveadapter.h` — curve → parameter,
  thread-safety (`Use Main Thread`).
- `sequencetrackcurve.h` / `sequencetracksegmentcurve.h` — the float curve track + `FCurve`
  segments (linear/bezier, `getValue`).
- `sequenceservice.h` — factory registrations required only by Approach B.
- `sequenceeditor.h` — the mutex-guarded edit discipline to mirror if an independent clock
  is ever used.
