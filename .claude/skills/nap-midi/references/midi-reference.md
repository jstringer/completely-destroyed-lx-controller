# NAP MIDI JSON & API reference (verified, NAP 0.8.0)

API from `<SDK>/system_modules/napmidi/include`; blocks from `<SDK>/demos/oscmidi`.

## JSON

MidiInputPort (top-level `Device`; empty `Ports` = open all devices) —
`<SDK>/demos/oscmidi/data/oscmidi.json:37-42`:
```json
{ "Type": "nap::MidiInputPort", "mID": "MidiInputPort", "Ports": [], "EnableDebugOutput": true }
```

MidiInputComponent (on an entity; empty filters + all type-bools = wildcard listener) —
`oscmidi.json:7-20`:
```json
{
    "Type": "nap::MidiInputComponent", "mID": "MidiInput",
    "Ports": [], "Channels": [], "Numbers": [],
    "NoteOn": true, "NoteOff": true, "Aftertouch": true, "ControlChange": true,
    "ProgramChange": true, "ChannelPressure": true, "PitchBend": true
}
```
Property keys (`midiinputcomponent.h:31-40`): `Ports`/`Channels`/`Numbers` (empty = match all) and the
seven booleans (all default `true`). **Casing:** `"Aftertouch"` (member is `mAfterTouch`).
`MidiInputPort` keys (`midiinputport.h:65-66`): `Ports` (empty = all), `EnableDebugOutput`.

## MidiEvent (`midievent.h`)

`using MidiValue = short;`. Event types (`Type` enum, `:35-44`): `noteOff (0x80)`, `noteOn (0x90)`,
`afterTouch (0xA0)`, `controlChange (0xB0)`, `programChange (0xC0)`, `channelPressure (0xD0)`,
`pitchBend (0xE0)`. Wildcards: `MIDI_VALUE_OMNI = -1` (and `MIDI_NUMBER_OMNI`/`MIDI_CHANNEL_OMNI`).

Accessors (`midievent.h:69-79`):
`getType()`, `getNumber()`, `getValue()`, `getNoteNumber()`, `getVelocity()`, `getCCNumber()`,
`getCCValue()`, `getPitchBendValue()` (float, ~-1..1), `getProgramNumber()`, `getChannel()`,
`getPort()` (the port resource `mID`, **not** the OS device name), and `toString()`.

## C++ connect + handle (`<SDK>/demos/oscmidi/module/src/midihandler.{h,cpp}`)

```cpp
// midihandler.h — slot member
Slot<const MidiEvent&> eventReceivedSlot = { this, &MidiHandlerComponentInstance::onEventReceived };

// midihandler.cpp — init(): declare dependency + connect
// getDependentComponents(...) { components.emplace_back(RTTI_OF(nap::MidiInputComponent)); }
auto* midi_input = getEntityInstance()->findComponent<MidiInputComponentInstance>();
if (!errorState.check(midi_input != nullptr, "%s: missing MidiInputComponent", mID.c_str())) return false;
midi_input->messageReceived.connect(eventReceivedSlot);
```

Branch on `event.getType()` and read the matching value (`getCCValue` for controlChange,
`getVelocity` for note on/off, `getValue` for channelPressure, `getPitchBendValue` for pitchBend);
`getNumber()`/`getChannel()` identify the control. The oscmidi demo itself only calls
`event.toString()` — the type-switch accessor pattern is standard usage of the header API above.

## Output (`midiport/midioutputport.h`)

`class NAPAPI MidiOutputPort : public Device` — send with `void sendEvent(const MidiEvent& event);`
(`:52`); properties `AllowFailure`, `Port` (`:59-60`). **There is no `MidiOutputComponent`** — output
is only through the port resource.

## Enumeration & the limitations

- `MidiInputPort::getPortNames()` (`midiinputport.h:59`) — names this port opened, comma-joined; a
  **startup snapshot**. `getPortNumbers()` too.
- **No hot-plug / live enumeration** anywhere in `napmidi`: the port binds devices in `start()`,
  never rescans, and there's no device-changed signal. Reconnecting a device requires restart.
- `nap::MidiPortInfo` (`midiport/midiportinfo.h:18`) is the only system-wide device poller, but it's
  declared **without `NAPAPI`** — not exported, so app code linking `napmidi.dll` can't use it.
  Practical enumeration: open all (`Ports: []`) + `getPortNames()`, or `EnableDebugOutput` to log.

## Flow & threading

device → `MidiInputPort` (RtMidi callback) → `MidiService::enqueueEvent` (lock-free queue) →
`MidiService::update()` on the **main thread** → matched `MidiInputComponentInstance::trigger` →
`messageReceived`. Handlers run on the update thread (no extra locking; ≤1 frame latency).

## Required modules

`napmidi`.
