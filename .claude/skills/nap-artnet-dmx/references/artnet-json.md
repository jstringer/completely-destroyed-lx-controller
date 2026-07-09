# NAP Art-Net JSON & API reference (verified, NAP 0.8.0)

API from `<SDK>/system_modules/napartnet/include`; blocks from `<SDK>/demos/artnetsend` and
`demos/artnetreceive`. Get exact keys with `nap_usage.py <Type>`.

## Sender: ArtNetController (top-level Device resource)

`<SDK>/demos/artnetsend/data/artnetsend.json:3-15`:

```json
{
    "Type": "nap::ArtNetController", "mID": "ArtNetController",
    "Subnet": 0, "Universe": 0,
    "IPAddress": "",
    "WaitTime": 2.0, "Frequency": 44,
    "Mode": "Broadcast", "UnicastLimit": 10, "Verbose": false, "Timeout": 2.0
}
```
Properties (`artnetcontroller.h:134-143`): `Subnet` (0-15), `Universe` (0-15), `Frequency` (Hz, max
`nap::artnet::refreshRate = 44`), `WaitTime` (force-resend seconds), `Mode` (`Broadcast`/`Unicast`),
`UnicastLimit`, `Verbose`, `Timeout` (Unicast only), `ChannelCount` (2-512, default 512, omitted
above), and an IP-address property. **IP key caveat:** the send demo writes `"IPAddress"` while the
header doc-comment says `'IP Address'` — the two disagree and the `.cpp` RTTI (authoritative) isn't
on disk, so copy the key from a working block and prefer leaving IP empty (auto-selects an adapter).
This is a `Device`, placed at top level in `Objects`, **not** on an Entity.

## Receiver: ArtNetReceiver + ArtNetInputComponent

Receiver (top-level Device) — `demos/artnetreceive/data/artnetreceive.json:3-8`:
```json
{ "Type": "nap::ArtNetReceiver", "mID": "ArtNetReceiver", "IP Address": "", "Port": 6454 }
```
(Here the key is `"IP Address"` with a space — a different class from the controller.)

Input component (+ a handler) on an Entity — `artnetreceive.json:9-28`:
```json
{
    "Type": "nap::Entity", "mID": "ArtNetEntity",
    "Components": [
        { "Type": "nap::ArtNetInputComponent", "mID": "Input",
          "Net": 0, "SubNet": 0, "Universe": 0, "Receive All": true },
        { "Type": "nap::MyArtNetHandlerComponent", "mID": "Handler", "Input": "./Input" }
    ],
    "Children": []
}
```
`ArtNetInputComponent` props (`artnetinputcomponent.h:32-35`): `Net`, `SubNet`, `Universe` (uint8),
`Receive All` (bool). The handler is your own component (see `nap-adding-a-module-class`);
`napartnet` provides no storage component. Its `Input` is a `ComponentPtr<ArtNetInputComponent>`
(a `./`-relative path here).

## send() overloads (`artnetcontroller.h:84-112`)

```cpp
void send(const FloatChannelData& channelData, int channelOffset = 0);  // floats 0.0-1.0 at offset
void send(float channelData, int channel);                             // single float, channel 0-511
void send(const ByteChannelData& channelData, int channelOffset = 0);  // bytes 0-255 at offset
void send(uint8 channelData, int channel);                             // single byte, channel 0-511
void clear();                                                          // all channels -> 0
```
`FloatChannelData = std::vector<float>`, `ByteChannelData = std::vector<uint8>`. `channelOffset +
size > ChannelCount` **asserts**. Send is deferred to `ArtNetService::update()`.

Send loop (`demos/artnetsend/src/artnetsendapp.cpp:167-174`): per channel `i` (0-based),
`mArtNetController->send(uint8_t(value), i);` — UI labels are 1-based cosmetics over 0-based calls.

## ArtNetEvent (`artnetevent.h`)

Read API (all return `uint8`): `getChannelCount()` (= data size), `operator[](i)` / `getChannelByIndex(i)`
(0-based), `getChannelByNumber(n)` (1-based, = index n-1), `getChannelData()` (raw `const uint8*`).
Stream identity: `getNet()`, `getSubNet()`, `getUniverse()`, `getPortAddress()` (15-bit),
`getSequence()`, `getPhysical()`.

Handler pattern (`demos/artnetreceive/module/src/artnethandler.cpp`): a
`Slot<const ArtNetEvent&>`, connected in `init()` with `mInput->packetReceived.connect(slot)`; the
handler copies `event[i]` into a buffer. Received bytes are raw magnitude (0-255) — normalize by /255
yourself if you need 0-1.

## Gotchas recap

Byte (0-255) vs normalized float (0.0-1.0); 0-based send channels; out-of-range asserts; deferred
transmit (≤44 Hz + `WaitTime` keepalive); Net ignored on send; controller binds `0.0.0.0:6454` so
pin the receiver's IP for same-machine loopback.

## Required modules

`napartnet` (send). Receiving in the demo uses a demo-local module wrapping it; the module you need is
`napartnet` plus your receiver/handler wiring.
