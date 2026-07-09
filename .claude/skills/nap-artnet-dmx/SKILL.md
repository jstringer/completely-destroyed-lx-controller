---
name: nap-artnet-dmx
description: Use when sending or receiving DMX over Art-Net with NAP's napartnet module — declaring an ArtNetController (sender) or ArtNetReceiver + ArtNetInputComponent (receiver) in a data json, calling ArtNetController::send() to output channel values, or reading ArtNetEvent channel data. Covers universe/subnet addressing, byte-vs-float value domains, and the deferred send model.
---

# NAP Art-Net / DMX (napartnet)

## Overview

Art-Net is DMX-512-over-UDP. NAP mediates it through `ArtNetService`, with a **send** path
(`ArtNetController`) and a **receive** path (`ArtNetReceiver` + `ArtNetInputComponent`). You address
by subnet+universe, write channel values as bytes (0-255) or normalized floats (0.0-1.0), and the
service transmits on its own `update()`, throttled to the Art-Net refresh rate.

## When to use

- Declaring an `ArtNetController` (to send) or `ArtNetReceiver`+`ArtNetInputComponent` (to receive).
- Sending DMX values (`ArtNetController::send`) or reading received channels (`ArtNetEvent`).
- Debugging addressing, value ranges, or send/receive on one machine.

## Mental model

- **`ArtNetController`** — a `nap::Device` **top-level resource** (not on an entity). One controller
  = one subnet+universe, up to `ChannelCount` (2-512) channels. `send()` stages data; the service
  transmits later.
- **`ArtNetReceiver`** — a `nap::Device` top-level resource (an IP + port). **`ArtNetInputComponent`**
  goes on an entity (spawned by a Scene) and filters by Net/SubNet/Universe (or `Receive All`),
  exposing a `packetReceived` signal carrying an `ArtNetEvent`.
- **Addressing:** sending uses an 8-bit address = subnet(4 bits, 0-15) + universe(4 bits, 0-15).
  **Net is ignored on send** (library limitation); receiving *can* filter on Net.
- **Two value domains:** byte overloads take 0-255; float overloads take normalized 0.0-1.0 (converted
  to bytes internally). Received `ArtNetEvent` data is always bytes (`uint8`).

## Send (C++)

Fetch the controller in `init()` (`findObject<ArtNetController>("…")`), then per frame:

```cpp
mController->send(uint8_t(value), channel);        // single byte 0-255 to channel 0-511
mController->send(0.5f, channel);                  // single normalized float 0.0-1.0
mController->send(byteVector, channelOffset);      // a block, starting at channelOffset
```

Channels are **0-based** in the send API. `send()` only stages; the service transmits on its own
`update()` (max ~44 Hz), and re-sends every `WaitTime` seconds even without changes.

## Receive (C++)

Connect a slot to the input component's `packetReceived` and read the event (all getters return
`uint8`):

```cpp
mInput->packetReceived.connect(mSlot);        // in init(); mInput is a ComponentInstancePtr<ArtNetInputComponent>
// in the handler:
for (int i = 0; i < event.getChannelCount(); i++) uint8_t v = event[i];   // operator[] is 0-based
```

`getChannelByIndex(i)` is 0-based; `getChannelByNumber(n)` is 1-based. `getUniverse()`/`getSubNet()`/
`getNet()` identify the stream.

## JSON shape

Copy exact blocks: `python ../nap-skill-authoring/scripts/nap_usage.py nap::ArtNetController` (and
`nap::ArtNetInputComponent`). Full blocks + `send`/`ArtNetEvent` API in **`references/artnet-json.md`**.

## Common mistakes

- **Byte vs float domain** — bytes are 0-255, floats 0.0-1.0. Received data is always bytes; if you
  store it as float without dividing by 255, your scale is 0-255 (as the receive demo does).
- **Out-of-range send asserts** — `channelOffset + size > ChannelCount` (default 512) hard-asserts,
  not a soft error. Channel index must be 0-511.
- **Send is deferred** — nothing goes on the wire until `ArtNetService::update()`; calling `send`
  faster than `Frequency` is fine and cheap.
- **Net ignored on send** — you can't target a specific Net when sending; only subnet+universe.
- **Send+receive on one machine** — the controller binds `0.0.0.0:6454`; give the receiver a specific
  IP so it doesn't collide.
- **Exact JSON key for IP varies** — don't guess it; copy from a working block (`nap_usage.py`) and
  prefer leaving IP empty (auto-selects an adapter) unless you must pin it.

## Verify

`nap-validate-data-json`, then `nap-build-run-verify`. Worked examples: `<SDK>/demos/artnetsend`
(controller + per-channel send loop), `<SDK>/demos/artnetreceive` (receiver + input component +
handler reading `ArtNetEvent`). Details: **`references/artnet-json.md`**.
