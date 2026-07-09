---
name: nap-midi
description: Use when working with NAP's napmidi module — declaring a MidiInputPort and MidiInputComponent (with message-type/channel/number filters) in a data json, connecting to the messageReceived signal and reading a MidiEvent in C++, sending via MidiOutputPort, or listing devices. Covers the input flow, message filtering, and the no-hot-plug-enumeration limitation.
---

# NAP MIDI (napmidi)

## Overview

MIDI flows: a physical device → **`MidiInputPort`** (a `Device` that opens devices) → **`MidiService`**
(lock-free queue, processed on the main thread) → matched **`MidiInputComponent`** filters →
`messageReceived` signal → your slot. You declare the port + a filtered component in json, then
connect a slot in C++ and read the `MidiEvent`.

## When to use

- Declaring a `MidiInputPort` and a filtered `MidiInputComponent`.
- Connecting to `messageReceived` and reading note/CC/pitch-bend values.
- Sending MIDI (`MidiOutputPort`), or listing available devices.

## Mental model

- **`MidiInputPort`** — a top-level `Device` resource. `Ports: []` (empty) opens **all** devices.
- **`MidiInputComponent`** — on an entity; filters by `Ports`/`Channels`/`Numbers` (empty = all) and
  per-message-type booleans (`NoteOn`, `NoteOff`, `Aftertouch`, `ControlChange`, `ProgramChange`,
  `ChannelPressure`, `PitchBend`; all default `true`). An empty-everything component is a wildcard
  listener. Its instance exposes `nap::Signal<const MidiEvent&> messageReceived`.
- **Events arrive on the main thread** (during `MidiService::update()`), so handler slots need no
  extra locking — but there's up to one frame of latency.
- **Output** is via **`MidiOutputPort::sendEvent(...)`** only — there is **no** `MidiOutputComponent`.

## JSON shape

Wildcard listener (`<SDK>/demos/oscmidi/data/oscmidi.json`): a `MidiInputPort` with `Ports: []`, and a
`MidiInputComponent` with empty filters + all type-bools `true`. Copy exact blocks:
`python ../nap-skill-authoring/scripts/nap_usage.py nap::MidiInputComponent`. Full blocks + the
`MidiEvent` accessor list in **`references/midi-reference.md`**.

**Casing trap:** the property key is **`"Aftertouch"`** (lowercase t), even though the member is
`mAfterTouch`.

## Connecting & reading (C++)

```cpp
// in init(): find the input component on this entity and connect a slot
auto* midi_in = getEntityInstance()->findComponent<MidiInputComponentInstance>();
midi_in->messageReceived.connect(eventReceivedSlot);   // Slot<const MidiEvent&>
// in the handler, branch on type and read the right value:
switch (event.getType()) {
    case MidiEvent::Type::controlChange:  v = event.getCCValue();       break;
    case MidiEvent::Type::noteOn:         v = event.getVelocity();      break;
    case MidiEvent::Type::pitchBend:      pb = event.getPitchBendValue(); break; // -1..1
}
int number = event.getNumber(); int ch = event.getChannel();
```

Declare the dependency so init order is right: `getDependentComponents` emplaces
`RTTI_OF(nap::MidiInputComponent)`.

## Listing devices — and what you can't do

- `MidiInputPort::getPortNames()` returns the names **this port opened** (a comma-joined string) — a
  **startup snapshot**, not a system scan. Enable `EnableDebugOutput` on the port to log opened ports.
- **There is no hot-plug / live enumeration API.** A port binds its devices in `start()` and never
  rescans; devices plugged in after launch require a restart. `napmidi` sends no device-changed signal.
- The one system-wide "list all devices" helper, `nap::MidiPortInfo`, is **not `NAPAPI`-exported**, so
  app code linking against `napmidi.dll` can't call it. Practical enumeration = open all (`Ports: []`)
  and read `getPortNames()`.

## Common mistakes

- **`"AfterTouch"`** instead of `"Aftertouch"` in json → the filter bool is silently not set.
- **Expecting hot-plug** — it's a startup snapshot; no rescan, no notification (see above).
- **`getPort()` returns the resource `mID`**, not the OS device name; `Ports` filters use the port
  object's configured names, not raw hardware strings.
- **Reaching for a `MidiOutputComponent`** — it doesn't exist; use `MidiOutputPort::sendEvent`.
- **Reading the wrong value getter for the type** — use `getCCValue`/`getVelocity`/`getPitchBendValue`
  per `getType()`; `getValue` is the raw second data byte.

## Verify

`nap-validate-data-json`, then `nap-build-run-verify`. Worked example: `<SDK>/demos/oscmidi`
(`data/oscmidi.json` + `module/src/midihandler.{h,cpp}` for the connect/handle pattern). Details:
**`references/midi-reference.md`**.
