# NAP sequencer JSON & API reference (verified, NAP 0.8.0)

Blocks from `<SDK>/demos/sequencer`. Property keys are the exact serialized strings (some differ from
header doc-comments — see traps). Get more with `nap_usage.py nap::SequencePlayer`.

## The player + outputs + clock (Embedded)

`<SDK>/demos/sequencer/data/default.json:145-192`:

```json
{
    "Type": "nap::SequencePlayer",
    "mID": "SequencePlayer",
    "Default Show": "Default Show.json",
    "Create Sequence on Failure": true,
    "Outputs": [
        { "Type": "nap::SequencePlayerEventOutput", "mID": "SequencePlayerEventOutput" },
        { "Type": "nap::SequencePlayerCurveOutput", "mID": "Float Output",
          "Parameter": "Float1", "Use Main Thread": true },
        { "Type": "nap::SequencePlayerCurveOutput", "mID": "Vec3 Output",
          "Parameter": "Vec3", "Use Main Thread": true }
    ],
    "Clock": { "Type": "nap::SequencePlayerStandardClock", "mID": "Clock" }
}
```

- `Outputs` and `Clock` are **Embedded** — inline objects, defined only here.
- `Parameter` on a curve output is a **Default** id reference to a `Parameter` declared elsewhere
  (`nap-parameters`).
- `Default Show` is a **filename** (a show file under `data/sequences/`), not a pointer.
- Clocks: `nap::SequencePlayerStandardClock` (main thread, portable default),
  `nap::SequencePlayerIndependentClock` (own thread, prop `Frequency` Hz), and
  `nap::SequencePlayerAudioClock` (napsequenceaudio; the demo uses this — swap to
  `StandardClock` for a project with no audio).

## The editor + GUI (Default id references)

`default.json:132-144`:

```json
{ "Type": "nap::SequenceEditor", "mID": "SequenceEditor", "Sequence Player": "SequencePlayer" },
{ "Type": "nap::SequenceEditorGUI", "mID": "SequenceEditorGUI",
  "Sequence Editor": "SequenceEditor", "Render Window": "SequencerWindow",
  "Draw Full Window": true, "Hide Marker Labels": false }
```

## The show file (separate from objects.json)

`<SDK>/demos/sequencer/data/sequences/Default Show.json` holds the `Sequence`: `Sequence Tracks`
(`nap::SequenceTrackCurveFloat`/`…Vec2/Vec3/Vec4`, event tracks, audio tracks), `Sequence Markers`,
`Duration`. Each track has `Name`, **`"Output ID"`** (binds to an output `mID`), `Segments`,
`TrackHeight`. A curve track's segments hold `FloatFCurve` point data. Example binding chain:
`SequenceTrackCurveVec3` with `"Output ID": "Vec3 Output"` → the `SequencePlayerCurveOutput` whose
`mID` is `"Vec3 Output"` → its `"Parameter": "Vec3"`.

## Property-key traps (JSON key ≠ header doc-comment)

| Use in JSON | Header doc-comment says (stale) | Where |
|-------------|-------------------------------|-------|
| `"Default Show"` | 'Default Sequence' | `sequenceplayer.h:215` |
| `"Output ID"` | 'Assigned Output ID' | `sequencetrack.h:32` |

Also: `"Create Sequence on Failure"` = `mCreateEmptySequenceOnLoadFail` (default true);
`"Use Main Thread"` on curve outputs (default true); `SequenceEditor."Undo Steps"` (default 100).

## C++ (from `<SDK>/demos/sequencer/src/sequencerapp.cpp`)

- GUI show (`:142-144`), in `update()`: `mGuiService->selectWindow(mTimelineWindow);
  mSequenceEditorGUI->show();`
- Event output wiring (`init`, `:54-112`): `eventOutput->mSignal.connect(slot)` where `mSignal` is
  `nap::Signal<const SequenceEventBase&>` (`sequenceplayereventoutput.h:44`), fired on the main thread.
- Transport control API (`sequenceplayer.h`): `setIsPlaying` (`:85`), `setIsPaused` (`:91`),
  `setIsLooping` (`:97`), `setPlayerTime` (`:103`), `setPlaybackSpeed` (`:109`).

## Runtime construction (only if you build a player in C++)

The demo builds everything from JSON. If you construct a player at runtime with
`ResourceManager::createObject<T>()`, honor the leaves-first order: override `ParameterFloat`s →
`SequencePlayerCurveOutput`s → clock → `SequencePlayer` — calling `init(errorState)` on each, and
`start()` on the `SequencePlayer` (it's a `Device`) after `init()`. `SequencePlayerOutput::init`
self-registers with `SequenceService`; the clock's `start(Slot<double>&)` is invoked by the player.

## Required modules

`napsequence`, `napsequencegui` (the demo also uses the audio variants for its audio clock/track).
