---
name: nap-sequence
description: Use when working with NAP's napsequence/napsequencegui modules — setting up a SequencePlayer, its Outputs (curve/event) and Clock, a SequenceEditor + SequenceEditorGUI in a data json; showing the timeline GUI from C++; binding sequence tracks to parameters; or debugging sequence loading. Covers the player/output/clock/editor graph and the show file.
---

# NAP sequencer (napsequence + napsequencegui)

## Overview

A `SequencePlayer` (a `Device`) plays a **Sequence** (loaded from a separate *show file*, not from
`objects.json`) and drives its **Outputs** — a `SequencePlayerCurveOutput` pushes a track's value
into a `Parameter`; a `SequencePlayerEventOutput` fires a signal. A `SequenceEditor` edits the model
safely and a `SequenceEditorGUI` draws the timeline. Tracks bind to outputs by an id string.

## When to use

- Adding/editing a `SequencePlayer` + `Outputs` + `Clock`, or a `SequenceEditor`/`SequenceEditorGUI`
  in a data json.
- Showing the timeline GUI from `App::update()`.
- Binding a curve track to a parameter, or wiring an event output's signal in C++.
- Diagnosing a sequence that won't load or a track that drives nothing.

## Mental model

- `SequenceEditorGUI` → `SequenceEditor` → `SequencePlayer` → `Outputs[]` (+ a `Clock`).
- A `SequencePlayerCurveOutput` links a curve track to a `Parameter` (`nap-parameters`); a
  `SequencePlayerEventOutput` exposes a `Signal` you connect in C++.
- **A track binds to an output by matching the track's `"Output ID"` string to an output's `mID`.**
- The **Sequence** (tracks/segments/curves) lives in a show file under `data/sequences/*.json`,
  referenced from the player only by filename. Editing the *graph* and editing the *timeline content*
  are two different files.
- `SequencePlayer` is a `Device`: the resource manager `init()`s the whole graph, then `start()`s the
  player thread — so outputs/clock/parameters are ready before playback starts. **All-JSON setups
  need no `createObject`/manual init**; only a player you build at runtime in C++ needs the explicit
  leaves-first `init()`→`start()` ordering.

## JSON shape (exact keys matter)

Copy a working graph rather than typing it: `python ../nap-skill-authoring/scripts/nap_usage.py
nap::SequencePlayer` (and `nap::SequenceEditorGUI`). Full blocks in **`references/sequence-json.md`**.
The pointer kinds: `SequencePlayer.Outputs` and `.Clock` are **Embedded** (inline objects);
`SequencePlayerCurveOutput.Parameter`, `SequenceEditor."Sequence Player"`,
`SequenceEditorGUI."Sequence Editor"`/`"Render Window"` are **Default** (id strings).

**Two stale doc-comment traps — use the JSON key, not the header comment:**
- The player's show-file property is **`"Default Show"`** (header comment says "Default Sequence").
- A track's output binding is **`"Output ID"`** (header comment says "Assigned Output ID").

## Showing the timeline GUI (C++)

Like all NAP ImGui, in `update()`, after selecting the window:

```cpp
mGuiService->selectWindow(mTimelineWindow);
mSequenceEditorGUI->show();      // every frame; default arg spawns/uses its own window
```

Resolve `mSequenceEditorGUI` in `init()` via `findObject<SequenceEditorGUI>("…")`. Playback is
usually driven from the GUI transport; to control it in code use `SequencePlayer::setIsPlaying`,
`setIsPaused`, `setIsLooping`, `setPlayerTime`, `setPlaybackSpeed`.

## Common mistakes

- **`"Default Show"` / `"Output ID"`** written as the stale doc-comment names → silently ignored.
- **Track drives nothing** — its `"Output ID"` doesn't match an output `mID`, or a curve output's
  `Parameter` type is incompatible with the track type (`SequenceTrackCurveVec3` → a `ParameterVec3`).
- **Missing show file doesn't crash** — with `Create Sequence on Failure: true` (default) the player
  loads an empty timeline instead. A blank timeline often means a bad/missing show path.
- **GUI not interactive** — `selectWindow` not called before `show()`, or `show()` not called every
  frame from `update()`.
- **Clock choice changes threading** — `SequencePlayerStandardClock` runs on the main thread;
  `SequencePlayerIndependentClock` on its own thread at `Frequency` Hz. Keep curve outputs'
  `Use Main Thread: true` unless the consumer is thread-safe.

## Verify

`nap-validate-data-json` on the data json (note: the show file is separate — lint it too if it has
`Type`s), then `nap-build-run-verify`. Worked example: `<SDK>/demos/sequencer` (graph in
`data/default.json`, show file in `data/sequences/Default Show.json`, GUI-show in
`src/sequencerapp.cpp`). Details: **`references/sequence-json.md`**.
