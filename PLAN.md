# lxcontrol large-scale refactor — Programs / Triggers / Controllers / Effects / Modulators

## Context

lxcontrol currently drives three Eurolite strobes via a statically-declared rig (`data/objects.json`: `Fixture`/`FixtureType`/`FixtureChannel`/`FixtureChannelBinding` resources) plus runtime-authored Presets, EffectLayers, and MidiMappings owned by `lxcontrolService`. This is inflexible:

- Fixtures/channels/bindings are plain Resources wired by hard-coded mIDs in both JSON and `lxcontrolApp::init()` — no remapping or runtime customization.
- EffectLayers bake their fixture/channel targets in at creation time; a Preset can only choose *whether* an effect plays, never *where*.
- Presets conflate value snapshots, effect activation, and MIDI note triggers.

The refactor replaces the authoring model wholesale while keeping: MIDI input & device management (incl. hot-plug), Art-Net/DMX output, and Fixture = a collection of channels assigned to an `ArtNetController`.

## Decisions (confirmed with user)

| Decision | Choice |
|---|---|
| Parameter→channel resolution | **ChannelType matching** — Parameters declare a semantic channel role; mapping an Effect to a Fixture auto-binds params to matching channels |
| Channel mixing | **LTP** — latest-triggered claimant wins per channel; on stop, revert to next-most-recent active claimant, else base value |
| Rig authoring | **JSON-authored** — fixtures become Entities+Components in `objects.json`; runtime GUI edits effects/triggers/programs only. No `Scene::spawn` needed (unproven in this SDK — zero usages) |
| Modulator engine | **Built on napsequence** — every Modulator wraps a `SequencePlayer`; reuses the existing `SequenceEditorGUI` timeline; ADSR sustain implemented with seek/pause handling |
| Controller scoping | **Program-scoped** — a Controller only responds to MIDI while its Trigger belongs to the loaded Program |
| Old code/content | **Clean break** — delete old classes and discard `data/user_content.json` (rename unloadable file to `.bak` at startup) |

## Verified SDK facts the design rests on

- Multiple components of the same type per entity are supported (`EntityInstance::getComponentsOfType`, napscene `entity.h:143`; `getComponent` returns the first).
- `ArtNetController::send(float value, int channel)` float overload exists (napartnet `artnetcontroller.h:92`); deferred-send model unchanged.
- napsequence: `SequencePlayer` (Device: `setIsPlaying/setIsPaused/setIsLooping/setPlayerTime/setPlaybackSpeed`, signals incl. `edited`), `SequencePlayerCurveOutput` (→ `nap::Parameter`, `mUseMainThread`), `SequencePlayerStandardClock`, `SequenceEditor` with programmatic track building via `SequenceControllerCurve` (`addNewCurveTrack<float>/insertSegment/insertCurvePoint/changeCurveType(math::ECurveInterp::{Bezier,Linear,Stepped})/changeMinMaxCurveTrack`, `SequenceEditor::changeSequenceDuration`), `SequenceEditorGUI` (napsequencegui).
- Runtime-construction + persistence pattern proven in this repo: `ResourceManager::createObject<T>()` + `makeUniqueID` + manual `init()` leaves-first (+ `start()` for Devices); `rtti::serializeObjects` with every non-embedded object listed explicitly; JSON-loaded Devices are init'd/started automatically by `ResourceManager::loadFile`. `lxcontrolService::createEffectLayer()` (`module/src/lxcontrolservice.cpp:392-488`) is the direct construction template for the new Modulators; `save()`/`rebuildFromLoadedContent()` (`lxcontrolservice.cpp:201-241`, `136-198`) the persistence template.
- Current `EDmxChannelType` is a data-width enum (8/16-bit), not a semantic role — new role taxonomy required.
- All module pointer properties today are `EPropertyMetaData::Default` (id-string refs); the only Embedded property in play is the new `Mapping` (below) and `ParameterGroup`'s members (untouched).

---

## Design

### 1. Fixture ECS (JSON-authored, in `objects.json`)

One Entity per fixture: one `FixtureComponent` + 22 `FixtureChannelComponent`s. `FixtureDmxWriterComponent` is deleted — DMX writing folds into `FixtureComponentInstance::update()`. The shared-`FixtureType` indirection is dropped.

**`module/src/channelrole.h/.cpp`** — enums only:
- `enum class EChannelRole { Dimmer, Strobe, Red, Green, Blue, ColorMacro, SoundMode, Generic }` (+ `RTTI_BEGIN_ENUM`). RGB units expressed as Role=Red/Green/Blue + per-channel `UnitIndex` int (1–6 for the Eurolite's six SMD units; 0 = not unit-scoped). PresetColor→ColorMacro, AutoSound→SoundMode.
- `enum class EDmxChannelWidth { Value8, Value16MSB, Value16LSB }` (renamed from `EDmxChannelType`).

**`module/src/fixturechannelmapping.h/.cpp`** — `FixtureChannelMapping : Resource`, pure data: `Role` (EChannelRole, Required), `UnitIndex` (int, Default 0), `BaseParameter` (`ResourcePtr<ParameterFloat>`, Required, Default flag — id-ref to the existing `Strobe1_Dimmer_Base` etc.).

**`module/src/fixturechannelcomponent.h/.cpp`** — `FixtureChannelComponent : Component` + Instance:
- Resource props: `Name` (string, Required), `Offset` (int, Required), `Width` (EDmxChannelWidth, Default Value8), `Mapping` (`ResourcePtr<FixtureChannelMapping>`, **Required|Embedded** — written inline).
- Instance: `mAbsoluteChannel` (set by sibling FixtureComponent), cached role/unit/base-param, and the **LTP claim stack**:
  ```cpp
  struct ChannelClaim { uint64 mActivationId; const EffectParameter* mParam; int mComponent; };
  std::vector<ChannelClaim> mClaims;  // sorted ascending by mActivationId
  ```
  `pushClaim(id, param, component)` (insert sorted; replace same-activation), `removeClaims(id)`, `float resolveValue()` = `mClaims.empty() ? base->mValue : back().mParam->getComponentValue(back().mComponent)`, clamped 0–1. No `update()`.

**`module/src/fixturecomponent.h/.cpp`** — `FixtureComponent : Component` + Instance:
- Resource props: `DisplayName` (string, Default), `Controller` (`ResourcePtr<ArtNetController>`, Required, Default flag), `StartChannel` (int, Required).
- Instance `init()`: gather sibling channels via `getComponentsOfType<FixtureChannelComponentInstance>`, compute `mAbsoluteChannel = StartChannel + Offset`, validate no overlaps, then self-register: `getEntityInstance()->getCore()->getService<lxcontrolService>()->registerFixture(this)`; `onDestroy()` unregisters. `update()`: per channel `mController->send(channel->resolveValue(), channel->mAbsoluteChannel)`.

**objects.json sketch (Strobe1; Strobe2 StartChannel 22, Strobe3 44):**
```json
{ "Type": "nap::Entity", "mID": "Strobe1Entity",
  "Components": [
    { "Type": "nap::FixtureComponent", "mID": "Strobe1_Fixture",
      "DisplayName": "Strobe 1", "Controller": "Universe0", "StartChannel": 0 },
    { "Type": "nap::FixtureChannelComponent", "mID": "Strobe1_Ch_Dimmer",
      "Name": "Dimmer", "Offset": 0, "Width": "Value8",
      "Mapping": { "Type": "nap::FixtureChannelMapping", "mID": "Strobe1_Ch_Dimmer_Map",
                   "Role": "Dimmer", "UnitIndex": 0, "BaseParameter": "Strobe1_Dimmer_Base" } },
    { "Type": "nap::FixtureChannelComponent", "mID": "Strobe1_Ch_Unit1_R",
      "Name": "Unit1_R", "Offset": 2, "Width": "Value8",
      "Mapping": { "Type": "nap::FixtureChannelMapping", "mID": "Strobe1_Ch_Unit1_R_Map",
                   "Role": "Red", "UnitIndex": 1, "BaseParameter": "Strobe1_Unit1_R_Base" } }
    /* … 22 channels total: Dimmer, Strobe, Unit1..6 R/G/B (roles Red/Green/Blue, UnitIndex 1..6),
       PresetColor→ColorMacro, AutoSound→SoundMode … */
  ], "Children": [] }
```
Scene `Entities` += Strobe1/2/3Entity; `FixtureDriverEntity` removed. **Stays unchanged:** `Window`, `Universe0`, `MidiPort`, `MidiMonitorEntity`, camera/gnomon, the three `ParameterGroup`s (22 inline `ParameterFloat`s each) and three `ParameterGUI`s. **Deleted from JSON:** all `FixtureChannel`s, `FixtureType_EuroliteStrobe540_22ch`, all 66 `FixtureChannelBinding`s, the 3 `Fixture`s, `FixtureDriverEntity`.

### 2. Effect parameters (`module/src/effectparameter.h/.cpp`)

`EffectParameter : Resource` (abstract). Props: `Name` (Required). Runtime API: `getComponentCount()`, `getComponentRole(int)`, `appliesToUnit(int)`, `getComponentValue(int)` (current, post-modulation), `resetToBase()`, `setComponentValue(int,float)`; non-serialized `std::vector<float> mCurrentValues`. Subclasses (registered in one .cpp):
- `EffectFloatParameter` — `Role` (Required), `Units` (`vector<int>`, Default, empty = all units), `Value` (float). 1 component.
- `EffectColorParameter` — `Red`/`Green`/`Blue` (floats), `Units`. 3 components with roles Red/Green/Blue.
- `EffectToggleParameter` — `Role` (Required), `Units`, `Value` (bool → 0/1). 1 component.
- `DimmerParameter`, `StrobeParameter` — trivial `EffectFloatParameter` subclasses with pre-set Role (the spec's "pre-mapped types"); one-click adds in the GUI.

These deliberately do **not** wrap `nap::Parameter` — `ParameterFloat` survives only as the 66 per-channel base values (Fixtures tab) and as each Modulator's hidden output sink (below).

### 3. Modulators — built on napsequence

**`module/src/modulator.h/.cpp`** — `Modulator : Resource` (abstract base):
- Props: `Name` (Default), `Target` (`ResourcePtr<EffectParameter>`, Required, Default), `TargetComponent` (int, Default −1 = all components), `Min`/`Max` (floats, Default 0/1), `Blend` (`EModulatorBlend { Replace, Multiply, Add }`), `Player` (`ResourcePtr<SequencePlayer>`, Default), `OutputValue` (`ResourcePtr<ParameterFloat>`, Default — the hidden 0..1 sink fed by the player's `SequencePlayerCurveOutput`).
- Runtime API: `onTrigger()`, `onStop()`, `update(double dt)` (default no-op; ADSR overrides for sustain hold), `float raw()` (reads `OutputValue->mValue`), `value() = lerp(Min, Max, raw())`, `isFinished()`.
- Construction (service-side, ported from `createEffectLayer`): clock → hidden `ParameterFloat` → `SequencePlayerCurveOutput` (`mUseMainThread=true`) → `SequencePlayer` (`mSequenceFileName = "<modID>.json"`, `CreateEmptySequenceOnLoadFail`) `init()`+`start()` → `SequenceEditor` → `SequenceEditorGUI` → generate the sequence curve from the subtype's props via `SequenceControllerCurve` (`addNewCurveTrack<float>`, points, `changeCurveType`, `changeSequenceDuration`).
- Subtype props are **curve generators**: changing them regenerates the sequence; the timeline remains hand-editable per modulator (its `SequenceEditorGUI` behind a "Timeline" toggle, shown from `App::update()` after `selectWindow` — today's exact pattern). Hand edits are overwritten if generator props change afterward (documented behavior).

**`module/src/adsrmodulator.h/.cpp`** — `ADSRModulator`. Props: `Attack`/`Decay`/`Release` (seconds), `Sustain` (0–1), `Loop` (bool, false). Sequence: (0,0)→(A,1)→(A+D,S)→(A+D+R,0), duration A+D+R. Semantics: `onTrigger()` → `setPlayerTime(0)` + play; the service's per-frame `update()` pauses the player when playerTime ≥ A+D while still held (sustain hold — the accepted seek/pause approach); `onStop()` → unpause/seek to release start and play out; `Loop` → `setIsLooping(true)`, no sustain hold. `isFinished()` = reached end, not playing, not held.

**`module/src/lfomodulator.h/.cpp`** — `LFOModulator`. Props: `Shape` (`ELfoShape { Sine, Ramp, Triangle, Square, Pulse, Gaussian }`), `Frequency` (Hz), `PulseWidth` (0–1, Pulse), `GaussianWidth` (bell exponent), `PhaseOffset` (0–1), `Retrigger` (bool). Sequence: one canonical 1-second cycle of the shape (sampled points; Stepped/Linear interp where exact), looping; frequency applied via `setPlaybackSpeed(Frequency)` so speed changes don't rebuild the curve (reapplied from the prop on rebuild). Shape sampling uses `math::waveform()` (Sine/Square/Triangle), phase for Ramp, threshold for Pulse, `math::bell()` (napmath `mathutils.h:166`) for Gaussian. `onTrigger()`: if Retrigger → `setPlayerTime(PhaseOffset)`.

**`module/src/stepsequencemodulator.h/.cpp`** — `StepSequenceModulator`. Props: `Steps` (`vector<float>`), `Rate` (steps/sec), `Advance` (`EStepAdvance { Clock, Trigger }`), `Loop` (bool true), `Glide` (bool false). Sequence: one segment per step, Stepped interp (Linear when Glide), canonical 1 s/step with `setPlaybackSpeed(Rate)`. Clock mode: play looping. Trigger mode: paused; each `onTrigger()` seeks to the next step boundary (re-firing the owning Trigger steps the sequence).

### 4. Effect (`module/src/effect.h/.cpp`)

`Effect : Resource`. Props: `Name` (Required), `Parameters` (`vector<ResourcePtr<EffectParameter>>`, Default), `Modulators` (`vector<ResourcePtr<Modulator>>`, Default). Runtime: `trigger()`/`stop()` forward to modulators; `update(dt)`: params `resetToBase()`, then each modulator `update(dt)` and blends `value()` into its target component(s) per `Blend`, declaration order; `isFinished()` = all modulators finished (lets ADSR release ring out). **No fixture knowledge.**

### 5. Trigger hierarchy (`module/src/trigger.h/.cpp`)

```cpp
struct EffectFixtureBinding {                    // RTTI_BEGIN_STRUCT (pattern: Scene::RootEntity)
    ResourcePtr<Effect>      mEffect;            // "Effect" (Default)
    std::vector<std::string> mFixtureNames;      // "Fixtures" — fixture ENTITY mIDs ("Strobe1Entity")
};
class Trigger : public Resource {};              // "Name" (Required), "Bindings" (vector<EffectFixtureBinding>, Default)
class ControllerTrigger : public Trigger {};     // fired by its mapped TriggerController
class EnterTrigger : public Trigger {};          // fired on Program load
class ExitTrigger : public Trigger {};           // fired on Program unload
```
Firing is service-arbitrated (`fireTrigger`/`stopTrigger`) since activation ids/claims live there. Fixture references as entity-mID strings resolved at fire time ⇒ **no wiring replay on load** (kills the old `addOverride()` replay fragility).

### 6. Controller (`module/src/triggercontroller.h/.cpp`)

`TriggerController : Resource` (named to avoid collision with `ArtNetController`; GUI label "Controller"). Props: `Name` (Required), `Trigger` (`ResourcePtr<Trigger>`, Required, Default — the one-to-one mapping), `Mode` (`EControllerMode { Momentary, Toggle, FireOnly }`). Semantics in the service: Momentary — noteOn/CC≥64 fires, noteOff/CC<64 stops (edge-detected to avoid CC jitter); Toggle — each on-event toggles (non-serialized `mLatched`); FireOnly — each on-event (re)fires.

### 7. MIDI binding (`module/src/midicontrollerbinding.h/.cpp`)

`MidiControllerBinding : Resource` — replaces `MidiMapping`. Filter block ported verbatim from `midimapping.h/.cpp` (`Ports`/`Channels`/`Numbers` + 7 message-type bools + `matches(MidiEvent&)`), plus `Controller` (`ResourcePtr<TriggerController>`, Required, Default). Many bindings → one controller (the spec's many-to-one).

### 8. Program (`module/src/program.h/.cpp`)

`Program : Resource`. Props: `Name` (Required), `Triggers` (`vector<ResourcePtr<Trigger>>`, Default). Load/unload semantics in the service.

### 9. Reworked `lxcontrolService` (rewrite of `module/src/lxcontrolservice.h/.cpp`)

**Keeps:** `MidiHotplugMonitor` + port restart in `update()` (file untouched), wildcard-`MidiInputComponentInstance` subscription via member `Slot`, MIDI log/last-event/counter Learn support, `makeUniqueID()`, createObject/init construction, `serializeObjects` → `data/user_content.json`, `loadFile` on setup.

**New surface:**
- Fixture registry: `registerFixture/unregisterFixture/getFixtures` (populated by `FixtureComponentInstance::init` — no more hard-coded fixture mIDs in the app).
- Effects: `createEffect`, `addEffectParameter(Effect&, TypeInfo)`, `addModulator(Effect&, TypeInfo, EffectParameter& target)` (builds the full player graph per §3), `regenerateModulatorSequence(Modulator&)`, removes.
- Triggers: `createTrigger(TypeInfo, name)`, `setTriggerBindings`, `uint64 fireTrigger(Trigger&)`, `stopTrigger`, `isTriggerActive`.
- Controllers/MIDI: `createController(name, Trigger*, EControllerMode)`, `createBinding(learnedEvent, TriggerController&)`, removes/getters.
- Programs: `createProgram`, `setProgramTriggers`, `loadProgram` (fire old program's ExitTriggers & stop its activations → fire new EnterTriggers → arm its ControllerTriggers), `unloadProgram`, `getActiveProgram`.

**LTP state:**
```cpp
struct Activation { uint64 mId; Trigger* mTrigger; std::vector<Effect*> mEffects; bool mReleasing = false; };
std::vector<Activation> mActivations;  uint64 mNextActivationId = 1;
```
`fireTrigger`: retrigger = stop-then-replace existing activation; new id; for each binding × named fixture (registry lookup by entity mID) × its channels × the effect's parameter components: on role + unit match → `channel->pushClaim(id, &param, c)`; then `effect->trigger()`. `stopTrigger`: `effect->stop()`, mark `mReleasing`; claims stay until `isFinished()` (release-linger), then reaped in `update()` via `removeClaims(id)` on all registered channels. Sorted claim stack + `back()` read make LTP and revert-on-stop fall out of the data structure.

**`update(dt)`:** hot-plug poll → dedup live activations' effects → `effect->update(dt)` (reads modulator sinks already written by SequenceService curve outputs on main thread; ADSR sustain-hold check) → reap finished releasing activations.

**`onMidiEvent`:** log + learn capture (existing) → for each matching `MidiControllerBinding` → resolve controller → **only if its Trigger is in the active Program's `Triggers`** → apply Mode semantics → fire/stop.

**`setup()`** shrinks to `(MidiInputComponentInstance&, ResourcePtr<MidiInputPort>, ResourcePtr<RenderWindow>, ErrorState&)` — fixtures self-register; render window still needed for constructing `SequenceEditorGUI`s.

**Persistence:** `user_content.json` holds only runtime-authored types (never in objects.json ⇒ blanket `getObjects<T>` is safe on load, unlike the old ParameterFloat trap): Effects + their params/modulators + per-modulator player/clock/output/hidden-param/editor/editorGUI (mirror today's explicit save list at `lxcontrolservice.cpp:201-241`), Triggers, TriggerControllers, MidiControllerBindings, Programs. On load: `loadFile` (Devices auto-init/start) → populate lists via `getObjects<T>` → match editor/GUI to players via their refs (today's `rebuildFromLoadedContent` pattern) → reapply `setPlaybackSpeed` from Frequency/Rate props. Sequence show files live in `data/` per modulator as today. Unloadable old-format file: log, rename `.bak`, continue empty. Runtime state (activations, latches, active program) is not persisted — boot is dark until a Program loads.

### 10. App (`src/lxcontrolapp.h/.cpp` — GUI rewrite, same skeleton)

`init()` resolves window/scene/camera/ParameterGUIs (fixture lookups gone — registry-driven); `update()`: input → `selectWindow` → `drawMainUI()` → `show()` any open modulator timelines (existing pattern). Tabs **Fixtures | Effects | Triggers | Programs | MIDI**:
1. **Fixtures** — the three `ParameterGUI::show()` columns (manual base values, unchanged) + per-channel debug: resolved-value bar, claim count, claimed-vs-base marker (via `getFixtures()`).
2. **Effects** — effect list (+New/Delete); selected effect: parameter list ("Add: Dimmer | Strobe | Float | Color | Toggle") with role combo / unit checkboxes / value slider or `ColorEdit3`; modulator list ("Add: ADSR | LFO | StepSeq") with per-type quick controls (drag-floats, shape combo, step `VSliderFloat` row with current-step highlight), each regenerating the sequence on change; live `ProgressBar(raw())`; per-modulator **Timeline** toggle opening its `SequenceEditorGUI`; target-param/component/Min/Max/Blend combos; per-effect "Audition on all fixtures" toggle (transient service activation — testable before Triggers exist).
3. **Triggers** — list with type badge; create form (name + Controller/Enter/Exit combo); bindings table (Effect combo × fixture checkboxes from registry display names); **Fire/Stop** test buttons; active/releasing indicator.
4. **Programs** — list; membership checklist of Triggers; **Load/Unload**; active-program banner shown on every tab.
5. **MIDI** — device list + hot-plug status and live message log (unchanged); Controllers section: list (name, mode combo, ControllerTrigger combo), create/delete; per-controller **Learn** (existing counter/last-event workflow → `createBinding`, message type+number only, device-agnostic); bindings listed under each controller with delete.

### 11. Deleted (clean break)

`module/src/`: `fixture.*`, `fixturetype.*`, `fixturechannel.*`, `fixturechannelbinding.*`, `fixturedmxwritercomponent.*`, `effectlayer.*`, `effectlayeroverride.*`, `preset.*`, `presetmiditrigger.*`, `presetmanagercomponent.*`, `midimapping.*`, `miditriggercomponent.*` (its `EMidiTriggerAction` is replaced by `EControllerMode`), `midiparametercomponent.*`. Also `data/user_content.json` (+ stale `data/presets/` snapshot dirs). `midihotplugmonitor.*` stays. Manifests: `module/module.json` and `app.json` keep their current module lists (napsequence/napsequencegui/napimgui still required); `regenerate.bat` still required whenever `.cpp` files are added/removed.

---

## Implementation phases

Every phase ends buildable + runnable. Loop: (files added/removed? → `regenerate.bat`) → `build.bat` with **Napkin closed** → run app → read the ResourceManager load log → observe. Validate `objects.json` edits with the `nap-validate-data-json` skill flow before running. Use the `nap-adding-a-module-class` scaffolder for each new class and `nap-resource-graph`/`nap-artnet-dmx`/`nap-midi`/`nap-parameters`/`nap-sequence` skills when touching the respective areas.

**Phase 1 — Fixture ECS + demolition.**
Add `channelrole.*`, `fixturechannelmapping.*`, `fixturechannelcomponent.*`, `fixturecomponent.*`. Delete the 13 file pairs (§11) + `user_content.json`. Strip `lxcontrolservice.*` to: hot-plug, MIDI subscription + log/learn accessors, fixture registry, save/load stubs. Reduce app tabs to Fixtures + MIDI (log/devices). Rewrite the rig in `objects.json` (§1). 
*Verify:* clean load log; Fixtures tab sliders drive observable Art-Net output; MIDI log streams; hot-plug reconnects.

**Phase 2 — Effects + Parameters + Modulators + persistence v1.**
Add `effectparameter.*`, `modulator.*`, `adsrmodulator.*`, `lfomodulator.*`, `stepsequencemodulator.*`, `effect.*`. Service: effect/param/modulator CRUD incl. the player-graph construction ported from the old `createEffectLayer` (git history), sequence generation from props, effect update loop, audition path (exercises the claim stack minimally), save/rebuild. App: Effects tab incl. Timeline toggles. 
*Verify:* create an effect with each modulator type; quick-control edits regenerate curves visibly in the timeline; audition drives real DMX (LFO pulses a dimmer); restart → everything reloads and plays.

**Phase 3 — Triggers + full LTP.**
Add `trigger.*`. Service: activations, fire/stop, claim push/reap, release-linger. App: Triggers tab with Fire/Stop. 
*Verify:* fire A (dimmer LFO on Strobe1) then B (dimmer ADSR on Strobe1) → B wins; stop B → A resumes; stop A → base resumes; ADSR release rings out after Stop; save/reload round-trips `EffectFixtureBinding` (first vector-of-struct — inspect the JSON).

**Phase 4 — Controllers + MIDI bindings + Learn.**
Add `triggercontroller.*`, `midicontrollerbinding.*`. Service: `onMidiEvent` dispatch (arm unconditionally until Phase 5), CRUD + persistence. App: MIDI tab rebuild. 
*Verify (real device or loopMIDI):* learn a note onto Momentary → note-on fires / note-off stops; Toggle latches; two bindings → one controller; persists across restart.

**Phase 5 — Programs.**
Add `program.*`. Service: load/unload, Enter/Exit firing, program-scoped controller arming, final save/rebuild. App: Programs tab + banner. 
*Verify:* EnterTrigger lights on Load; ExitTrigger fires on program switch; ControllerTriggers respond only while their program is loaded; cold-start → reload all content, load program, play a full show.

## Verification (end-to-end, after Phase 5)

1. Fresh start with no `user_content.json`: author 2 effects (color LFO, dimmer ADSR), 3 triggers (Controller/Enter/Exit), 1 controller learned from a MIDI pad, 1 program containing them.
2. Restart the app: all content reloads; load the program: EnterTrigger fires; pad press/release drives the ADSR with visible release-linger; simultaneous triggers demonstrate LTP order and revert.
3. Watch DMX output (Art-Net monitor or the fixtures) and the Fixtures-tab claim debug for every step above.

## Risks / open questions

1. **ADSR sustain via pause/seek** — main-thread time watching of a threaded `SequencePlayer` has ~1-frame jitter at the sustain point; verify feel in Phase 2 (fallback: give ADSR a long flat sustain segment and only seek on release).
2. **Timeline hand-edits vs generator props** — prop changes overwrite hand edits; surface a "modified in timeline" hint if it bites.
3. **Service-vs-scene update order** — modulator sink values may be ≤1 frame stale at DMX-write time; imperceptible for lighting, confirm empirically in Phase 2.
4. **Shared Effect state across concurrent Triggers** — two active triggers on one Effect share modulator state; re-fire retriggers both. Accepted v1 behavior; per-activation effect instances if it bites.
5. **`vector<struct{ResourcePtr,…}>` serialization** (`EffectFixtureBinding`) — pattern exists in SDK (`Scene::RootEntity`); Phase 3 round-trip is the gate; fallback: promote the binding to a small standalone Resource.
6. **CC-driven Momentary jitter** — mitigate with threshold-crossing edge detection.
7. **Generic type names** (`nap::Effect/Trigger/Program`) — verified free in NAP 0.8.0; a future SDK upgrade could collide.
8. **No true resource deletion** (`ResourceManager` has no public remove) — deleted content lingers in memory until restart, excluded from save; unchanged known limitation.
9. **16-bit widths** — enum kept, only Value8 emitted (as today).
10. **objects.json duplication** — 3 × ~22 channel blocks with no shared FixtureType; accepted for real ECS channels (future generator script possible).
