# lxcontrol large-scale refactor — Programs / Triggers / Controllers / Effects / Modulators

## Audit revisions (2026-07-09)

Audited against the NAP 0.8.0 SDK headers and the current codebase via the repo's NAP skills/agents, ponytail-passed for over-engineering, then reconciled across two rounds of user direction on how to use `napsequence`. Where this lands:

1. **Every Modulator owns its own lightweight `SequencePlayer` + clock + a custom `napsequence` Output/Adapter — not the full multitrack curve editor.** The single-playhead constraint (`sequenceplayer.h:302`, one atomic `mTime`) is a non-issue because each modulator has its *own* player — so retriggered LFOs and independently-fired ADSRs all coexist. The simple modulators (ADSR, LFO, Step) deliver their value through the sequencer's Output/Adapter pipeline (**Route 2, user-chosen**): a custom `lx::ModulatorOutput` + `lx::ModulatorAdapter` + a bare `lx::ModulatorTrack`; the value is computed procedurally in `evaluate(playerTime)` inside `adapter.tick()` (clock thread), thread-hopped to a `ParameterFloat` sink on the main thread, with a custom **PlotLines** GUI. Only `lx::CurveModulator` uses the shipped `SequenceEditor` + `SequenceEditorGUI` + curve track(s) for hand-drawn multi-track curves.
2. **Clock per subtype:** `SequencePlayerStandardClock` (main thread) or `SequencePlayerIndependentClock` (own thread, `mFrequency` Hz) — ADSR/LFO can opt into the independent clock for jitter-free time; the adapter samples on that clock's thread and the value reaches the sink parameter on the main thread via the output's `update()`.
3. **ADSR sustain is procedural, not player-driven.** SDK has **no loop-range** (`setIsLooping` loops all of `[0,duration]`) and **no end/finished/wrap signal** (only `preTick`/`postTick`/`playerTimeChanged`, `sequenceplayer.h:163-212`). So attack→decay→sustain-hold→release lives in `evaluate()` over the player's clock — giving *exact* sustain (no pause/seek jitter), while still using the player as the timebase.
4. **Custom Output/Adapter is the chosen value path (Route 2).** An adapter is created by a factory **keyed by track type** (`SequenceService::registerAdapterFactoryFunc`, `sequenceservice.h:108`), so it needs a registered track — supplied automatically by a **default-track-creator registered for the output type** (`registerDefaultTrackCreatorForOutput`, `sequenceservice.h:58`), the same mechanism the current `createEffectLayer` relies on. No hand-authored show file, no editor, for ADSR/LFO/Step.
5. **Naming: a real `namespace lx`** (not an `Lx` prefix). `lx::Effect`, `lx::Modulator`, `lx::FixtureComponent`, … ; `"Type": "lx::Effect"` in JSON. RTTI bases and flag enums stay `nap::`-qualified. Verified: RTTI type strings are the fully-qualified C++ name; nothing in the load path assumes `nap::`.
6. Non-controversial fixes kept: `mAbsoluteChannel` computed at write time (no sibling init-order write), delete-path claim guard, SDK signature corrections.

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
| Modulator engine | **Per-modulator `SequencePlayer` + custom Output/Adapter (Route 2).** Each `lx::Modulator` owns a player + clock (Standard/Independent per subtype) + a custom `lx::ModulatorOutput`/`lx::ModulatorAdapter`/`lx::ModulatorTrack`; ADSR/LFO/Step compute procedurally in `adapter.tick()` → `ParameterFloat` sink + PlotLines GUI; `lx::CurveModulator` alone uses the shipped `SequenceEditor`/GUI + curve tracks. |
| Type naming | **`namespace lx`** for all new module classes (`lx::Effect`, `lx::Modulator`, …); `nap::` kept only on RTTI bases + flag enums |
| Controller scoping | **Program-scoped** — a Controller only responds to MIDI while its Trigger belongs to the loaded Program |
| Old code/content | **Clean break** — delete old classes and discard `data/user_content.json` (rename unloadable file to `.bak` at startup) |

## Verified SDK facts the design rests on

Verified against NAP 0.8.0 headers (file:line). SDK is headers-only; where behavior lives in the absent `.cpp`, it is inferred from header + doc comments and noted.

- Multiple components of the same type per entity: `EntityInstance::getComponentsOfType` (all), `getComponent` (first) — napscene `entity.h:128,143`.
- **Component init-order caveat:** a `ComponentInstance::init()` can reach siblings (`getEntityInstance()->getComponent<T>()`) and services (`getEntityInstance()->getCore()->getService<T>()`, `core.h:240`), but a sibling is only guaranteed `init()`-ed first via `getDependentComponents()`. ⇒ the fixture reads sibling **resource** props (always available), never sibling instance state, during init (§1).
- `ArtNetController::send(float value, int channel)` float overload — napartnet `artnetcontroller.h:92`.
- `math::waveform(math::EWaveform, float time, float frequency)` at napmath `waveform.h:32`; **uppercase** `EWaveform { SINE, SQUARE, SAW, TRIANGLE }` (LFO "Ramp" ⇒ `SAW`). `math::bell(time, strength)` at `mathutils.h:166`. `math::lerp` available.
- **napsequence — clock, player, outputs/adapters, GUI:**
  - Clock: `SequencePlayerClock` (base, `sequenceplayerclock.h:26`) → `SequencePlayerStandardClock` (:59, ctor `SequenceService&`, main thread) / `SequencePlayerIndependentClock` (:104, own thread, `float mFrequency=1000.0f` :141). Contract: `start(Slot<double>& updateSlot)` (:43) / `stop()` (:48) — a clock just ticks a `Slot<double>` (deltaTime); the player wires its own slot.
  - `SequencePlayer` (Device, `sequenceplayer.h`): `setIsPlaying/getIsPlaying` (85/124), `setIsPaused/getIsPaused` (91/134), `setIsLooping/getIsLooping` (97/129 — **loops all of [0,duration] only; no loop range**), `setPlayerTime(double)/getPlayerTime()/getDuration()` (103/114/119), `setPlaybackSpeed/getPlaybackSpeed` (109/139). Single `std::atomic<double> mTime` (:302). Props: `mSequenceFileName` (215), `mCreateEmptySequenceOnLoadFail=true` (216), `mOutputs` vector (217), `mClock` (218). **No `SequenceEditor` reference** — the editor points at the player, not vice-versa.
  - Signals (`sequenceplayer.h:163-212`): `playbackSpeedChanged`, `playerTimeChanged`, `playStateChanged`, `pauseStateChanged`, `preTick`/`postTick` (player thread, around adapters), `sequenceLoaded`, `edited`. **No finished / end-reached / loop-wrap signal.**
  - Outputs/Adapters: `SequencePlayerOutput` (`sequenceplayeroutput.h:23`, ctor `SequenceService&`, self-registers in `init()`, `onDestroy()` removes; one virtual `update(double)` :57 called on main thread by `SequenceService::update`). `SequencePlayerAdapter` (`sequenceplayeradapter.h:26`, plain class, `tick(double time)` :38 + `destroy()` :43). Adapter factory keyed by **track type**: `SequenceService::registerAdapterFactoryFunc(TypeInfo, factory)` (:108) / `invokeAdapterFactory` (:118); player builds adapters at `start()` from each track's `mAssignedOutputID` (`sequencetrack.h:32`). ⇒ a custom adapter needs a registered track type to exist. `SequencePlayerCurveOutput`→`Parameter` + `mUseMainThread` (`sequenceplayercurveoutput.h:32`).
  - Editor/GUI (only for `lx::CurveModulator`): `SequenceEditor` ctor `SequenceService&`, is the safe-mutation front (`sequenceeditor.h:45-48`); `SequenceControllerCurve` via `editor.getController<SequenceControllerCurve>()` — `addNewCurveTrack<float>()` (templated), `insertSegment(trackID, double)` (**NOT templated** → `const SequenceTrackSegment*`), `insertCurvePoint(trackID, segmentID, float pos, int curveIndex)`, `changeCurveType(trackID, segmentID, math::ECurveInterp, curveIndex)`, `changeMinMaxCurveTrack<float>`. `SequenceEditor::changeSequenceDuration(double)`. `SequenceEditorGUI` ctor `SequenceGUIService&`; props `mRenderWindow` (:62)/`mSequenceEditor` (:63)/`mDrawFullWindow` (:64); `show(bool)` (:54); `drawTracks` renders all tracks on one timeline. **No read-only mode / reusable standalone curve widget** — the procedural modulators draw `ImGui::PlotLines` instead.
- RTTI/namespace: existing module classes register at global scope with fully-qualified names (`RTTI_BEGIN_CLASS(nap::MidiMapping)` in `midimapping.cpp:5`, class body in a separate `namespace nap {}`). A `namespace lx` works identically: `RTTI_BEGIN_CLASS(lx::Effect)` → type string `"lx::Effect"`.
- Runtime-construction + persistence proven in-repo: `ResourceManager::createObject<T>()` + `makeUniqueID` + manual `init()` leaves-first (+ `start()` for Devices); `rtti::serializeObjects` with every non-embedded object listed explicitly. `lxcontrolService::createEffectLayer()` is the construction template (cite by behavior — line numbers shifted by the collaborator's push); `save()`/`rebuildFromLoadedContent()` the persistence template.
- `vector<struct-holding-ResourcePtr+vector>` IS serializable (`Scene::mEntities` = `std::vector<RootEntity>`, `scene.h:27-33`). **Medium confidence:** literal `RTTI_BEGIN_STRUCT` macro unreadable ⇒ `lx::EffectFixtureBinding` round-trip is a Phase-3 gate (§Risks #3).

---

## Design

**Naming:** every new module class lives in `namespace lx`. Enums keep the `E` convention inside `lx` (`lx::EChannelRole`, …). JSON `"Type"` uses the qualified name (`"nap::Entity"` for SDK types, `"lx::FixtureComponent"` for ours).

### 1. Fixture ECS (JSON-authored, in `objects.json`)

One Entity per fixture: one `lx::FixtureComponent` + 22 `lx::FixtureChannelComponent`s. `FixtureDmxWriterComponent` is deleted — DMX writing folds into `lx::FixtureComponentInstance::update()`. The shared-`FixtureType` indirection is dropped.

**`module/src/channelrole.h/.cpp`** — enums only:
- `enum class EChannelRole { Dimmer, Strobe, Red, Green, Blue, ColorMacro, SoundMode, Generic }` (+ `RTTI_BEGIN_ENUM`). RGB units = Role Red/Green/Blue + per-channel `UnitIndex` int (1–6 for the six SMD units; 0 = not unit-scoped). PresetColor→ColorMacro, AutoSound→SoundMode.
- `enum class EDmxChannelWidth { Value8, Value16MSB, Value16LSB }` (renamed from `EDmxChannelType`).

**`module/src/fixturechannelmapping.h/.cpp`** — `lx::FixtureChannelMapping : Resource`, pure data: `Role` (Required), `UnitIndex` (int, Default 0), `BaseParameter` (`ResourcePtr<ParameterFloat>`, Required, Default flag — id-ref to `Strobe1_Dimmer_Base` etc.).

**`module/src/fixturechannelcomponent.h/.cpp`** — `lx::FixtureChannelComponent : Component` + Instance:
- Resource props: `Name` (Required), `Offset` (int, Required), `Width` (EDmxChannelWidth, Default Value8), `Mapping` (`ResourcePtr<FixtureChannelMapping>`, **Required|Embedded** — inline).
- Instance: cached role/unit/base-param (from its own Mapping — available in its own `init()`), and the **LTP claim stack**:
  ```cpp
  struct ChannelClaim { uint64 mActivationId; const lx::EffectParameter* mParam; int mComponent; };
  std::vector<ChannelClaim> mClaims;  // sorted ascending by mActivationId
  ```
  `pushClaim` (insert sorted; replace same-activation), `removeClaims(id)`, `float resolveValue()` = `mClaims.empty() ? base->mValue : back().mParam->getComponentValue(back().mComponent)`, clamped 0–1. No cached absolute channel, no `update()`.

**`module/src/fixturecomponent.h/.cpp`** — `lx::FixtureComponent : Component` + Instance:
- Resource props: `DisplayName` (Default), `Controller` (`ResourcePtr<ArtNetController>`, Required, Default flag), `StartChannel` (int, Required).
- Instance `init()`: gather sibling channels via `getComponentsOfType<FixtureChannelComponentInstance>` (read-only — build the write list, validate no two channels' `Offset` ranges overlap using **resource** `Offset`/`Width`). Self-register: `getEntityInstance()->getCore()->getService<lxcontrolService>()->registerFixture(this)`; `onDestroy()` unregisters. No sibling-instance writes ⇒ no `getDependentComponents()`.
- `update()`: per channel `mController->send(channel->resolveValue(), mStartChannel + channel->getOffset())` — absolute channel at write time. `// ponytail: StartChannel+Offset at point of use; caching it is bug surface.`

**objects.json sketch (Strobe1; Strobe2 StartChannel 22, Strobe3 44):**
```json
{ "Type": "nap::Entity", "mID": "Strobe1Entity",
  "Components": [
    { "Type": "lx::FixtureComponent", "mID": "Strobe1_Fixture",
      "DisplayName": "Strobe 1", "Controller": "Universe0", "StartChannel": 0 },
    { "Type": "lx::FixtureChannelComponent", "mID": "Strobe1_Ch_Dimmer",
      "Name": "Dimmer", "Offset": 0, "Width": "Value8",
      "Mapping": { "Type": "lx::FixtureChannelMapping", "mID": "Strobe1_Ch_Dimmer_Map",
                   "Role": "Dimmer", "UnitIndex": 0, "BaseParameter": "Strobe1_Dimmer_Base" } },
    { "Type": "lx::FixtureChannelComponent", "mID": "Strobe1_Ch_Unit1_R",
      "Name": "Unit1_R", "Offset": 2, "Width": "Value8",
      "Mapping": { "Type": "lx::FixtureChannelMapping", "mID": "Strobe1_Ch_Unit1_R_Map",
                   "Role": "Red", "UnitIndex": 1, "BaseParameter": "Strobe1_Unit1_R_Base" } }
    /* … 22 channels total: Dimmer, Strobe, Unit1..6 R/G/B (Red/Green/Blue, UnitIndex 1..6),
       PresetColor→ColorMacro, AutoSound→SoundMode … */
  ], "Children": [] }
```
Scene `Entities` += Strobe1/2/3Entity; `FixtureDriverEntity` removed. **Unchanged:** `Window`, `Universe0`, `MidiPort`, `MidiMonitorEntity`, camera/gnomon, the three `ParameterGroup`s (22 inline `ParameterFloat`s each — base params `Strobe1_Dimmer_Base` etc., **verified**) and three `ParameterGUI`s. **Deleted from JSON:** all `FixtureChannel`s, `FixtureType_EuroliteStrobe540_22ch`, all 66 `FixtureChannelBinding`s, the 3 `Fixture`s, `FixtureDriverEntity`. **Class deletion (§11) + this rewrite land in one commit** (stale type ref = "Unknown object type").

### 2. Effect parameters (`module/src/effectparameter.h/.cpp`)

`lx::EffectParameter : Resource` (abstract). Props: `Name` (Required). Runtime API: `getComponentCount()`, `getComponentRole(int)`, `appliesToUnit(int)`, `getComponentValue(int)` (current, post-modulation), `resetToBase()`, `setComponentValue(int,float)`; non-serialized `std::vector<float> mCurrentValues`. **Authored base values are plain serialized float/bool props; `mCurrentValues` is the runtime post-modulation output. Modulators write only `mCurrentValues`, never the authored base; `resetToBase()` copies authored → `mCurrentValues` at the top of each `Effect::update()`.** All component values are normalized 0..1 (`FixtureChannelComponent::resolveValue()` clamps 0–1). Subclasses (one .cpp):
- `lx::FloatParameter` — `Role` (Required), `Units` (`vector<int>`, Default, empty = all), `Value` (float). 1 component.
- `lx::ColorParameter` — `Red`/`Green`/`Blue` (floats), `Units`. 3 components, roles Red/Green/Blue.
- `lx::ToggleParameter` — `Role` (Required), `Units`, `Value` (bool → 0/1). 1 component.
- `lx::DimmerParameter`, `lx::StrobeParameter` — trivial `FloatParameter` subclasses with pre-set Role (the "pre-mapped types"); one-click GUI adds.

**Deliberately do not compose `nap::Parameter` (evaluated and rejected, not "can't").** Single-parameter drawing *is* available — `ParameterGUIService::findEditor(const Parameter&)` (`parameterguiservice.h:63`) draws one param's registered editor with no `ParameterGroup` wrapper — so GUI reuse was possible. Rejected on complexity: composing a `ParameterFloat`/`ParameterRGBColorFloat`/`ParameterBool` per param adds an Embedded child resource, a `baseParam()` virtual, the `{"Values":[r,g,b]}` color value-shape + `getRed/Green/Blue` unpack, and manual `init()` ordering for the runtime-`createObject`'d embedded child — more total code than the one-line `SliderFloat` / `Checkbox` / `ColorEdit3(float[3])` it would replace, and the sole concrete win (min/max clamping) is void because every component is fixed 0..1. Multi-component RGB, roles, and unit scoping also don't map onto single-valued `nap::Parameter`. `ParameterFloat` survives as the 66 per-channel base values (Fixtures tab) and as `lx::CurveModulator`'s curve-output sink (§3). Modulators blend `value()` into `mCurrentValues` via `lx::Effect::update()` (§4).

### 3. Modulators — per-modulator `SequencePlayer` as clock substrate

**`module/src/modulator.h/.cpp`** — `lx::Modulator : Resource` (abstract base). The base **owns a `SequencePlayer` + clock + a `lx::ModulatorOutput` + a `ParameterFloat` sink** (built by the service; subtype chooses `SequencePlayerStandardClock` or `SequencePlayerIndependentClock`). The base abstracts all player/clock/output wiring — subtypes only implement `evaluate()` (+ GUI).
- Props: `Name` (Default), `Target` (`ResourcePtr<lx::EffectParameter>`, Required, Default), `TargetComponent` (int, Default −1 = all), `Min`/`Max` (Default 0/1), `Blend` (`EModulatorBlend { Replace, Multiply, Add }`), `Clock` (`EModulatorClock { Standard, Independent }`, Default per subtype), `ClockFrequency` (Hz, Independent only).
- Non-serialized runtime state: `bool mHeld`, `bool mReleasing`, `double mReleaseTime`, `float mReleaseFrom`.
- Runtime API: `onTrigger()` (`mPlayer->setPlayerTime(0)` if retrigger semantics; `mHeld=true`; `mReleasing=false`; `mPlayer->setIsPlaying(true)`), `onStop()` (`mHeld=false`; `mReleasing=true`; `mReleaseTime=0`; `mReleaseFrom=evaluate(now)`), `update(double dt)` (release bookkeeping; the clock advances `mTime` and drives `adapter.tick()`), `virtual float evaluate(double t) = 0` (0..1 from player time + held/release state — **called from `adapter.tick()` on the clock thread**), `float value() const { return math::lerp(mMin, mMax, mSink->mValue); }` (reads the thread-hopped sink), `virtual bool isFinished() const`, `virtual void drawGui()` (default: `ImGui::PlotLines` sampling `evaluate()` + quick controls).

**The three custom `napsequence` classes (shared by all simple modulators):**
- **`module/src/modulatortrack.h/.cpp`** — `lx::ModulatorTrack : nap::SequenceTrack`, empty subclass (`RTTI_ENABLE(nap::SequenceTrack)`, `RTTI_BEGIN_CLASS` no props). Inherits `mAssignedOutputID`/`mSegments`; carries **zero segments** — the adapter never reads them. Exists only to key the adapter factory.
- **`module/src/modulatoroutput.h/.cpp`** — `lx::ModulatorOutput : nap::SequencePlayerOutput` (ctor `(SequenceService&)`). Members: `ResourcePtr<ParameterFloat> mParameter` (the sink, "Parameter" prop), back-pointer `lx::Modulator* mModulator` (or `std::function<float(double)>`), `std::vector<ModulatorAdapter*> mAdapters` + `registerAdapter/removeAdapter`. Overrides `update(double)` (main thread, called by `SequenceService::update`) to flush each adapter's stored value into `mParameter`. Needs `ObjectCreator<lx::ModulatorOutput, nap::SequenceService>` (§9).
- **`module/src/modulatoradapter.h/.cpp`** — `lx::ModulatorAdapter : nap::SequencePlayerAdapter` (plain class, **no RTTI**). Members: `lx::ModulatorOutput& mOutput`, `std::mutex mMutex`, `float mStoredValue`. `tick(double t)` (clock thread): `float v = mOutput.mModulator->evaluate(t); {lock} mStoredValue = v;`. `flush()` (main thread, from output's `update()`): `{lock} mOutput.mParameter->setValue(mStoredValue);`. `destroy()`: `mOutput.removeAdapter(this)`. Mirrors `SequencePlayerCurveAdapter`'s `mUseMainThread=true` path exactly — never writes the Parameter off the main thread.

Simple subtypes implement `evaluate(t)`; the base's player runs an empty sequence (`CreateEmptySequenceOnLoadFail`) whose single `ModulatorTrack` is emitted by the registered default-track-creator, bound to the `ModulatorOutput`:
- **`lx::AdsrModulator`** — `Attack`/`Decay`/`Release` (s), `Sustain` (0–1), `Loop` (bool); default clock Independent (jitter-free envelope). `evaluate(t)` while held: `t<A`→`t/A`; `t<A+D`→`lerp(1,Sustain,(t-A)/D)`; else→`Sustain` (**held exactly**). On release: `lerp(mReleaseFrom,0,mReleaseTime/Release)`. `isFinished()` = `mReleasing && mReleaseTime>=Release` (&& `!Loop`). *(Sustain/release procedural over the clock — SDK has no loop-range or finished signal.)*
- **`lx::LfoModulator`** — `Shape` (`ELfoShape { Sine, Ramp, Triangle, Square, Pulse, Gaussian }`), `Frequency` (Hz), `PulseWidth`, `GaussianWidth`, `PhaseOffset`, `Retrigger`; default clock Standard. `evaluate(t)` = shape at `phase = frac(t*Frequency + PhaseOffset)` (Sine→SINE/Ramp→SAW/Triangle→TRIANGLE/Square→SQUARE via `math::waveform`; threshold for Pulse; `math::bell` for Gaussian), remapped 0..1. `Retrigger` → `onTrigger` seeks the (own) player to 0 — safe, per-modulator playhead.
- **`lx::StepModulator`** — `Steps` (`vector<float>`), `Rate` (steps/sec), `Advance` (`EStepAdvance { Clock, Trigger }`), `Loop`, `Glide`. Clock: `idx = int(t*Rate)`. Trigger: non-serialized `mStepIndex` advanced in `onTrigger()`. `evaluate` = `Steps[idx]` (lerp next if Glide).

**`lx::CurveModulator`** — the ONE type using the shipped editor instead of the custom output. Owns (besides player+clock) one or more `SequencePlayerCurveOutput`s → `ParameterFloat` sink(s), curve track(s), a `SequenceEditor`, and a `SequenceEditorGUI`, built per the `createEffectLayer` template (multi-track allowed). `value()` reads the curve output's sink parameter. `drawGui()` shows its `SequenceEditorGUI` (behind a per-modulator "Timeline" toggle, from `App::update()` after `selectWindow`). Hand-authored, not generated.

### 4. Effect (`module/src/effect.h/.cpp`)

`lx::Effect : Resource`. Props: `Name` (Required), `Parameters` (`vector<ResourcePtr<lx::EffectParameter>>`, Default), `Modulators` (`vector<ResourcePtr<lx::Modulator>>`, Default). Runtime: `trigger()`/`stop()` forward to modulators; `update(dt)`: params `resetToBase()`, then each modulator `update(dt)` and blend `value()` into target component(s) of `EffectParameter::mCurrentValues` per `Blend`, declaration order; `isFinished()` = all modulators finished (lets ADSR release ring out). **No fixture knowledge.**

### 5. Trigger hierarchy (`module/src/trigger.h/.cpp`)

```cpp
namespace lx {
struct EffectFixtureBinding {                    // RTTI_BEGIN_STRUCT (pattern: Scene::RootEntity)
    ResourcePtr<lx::Effect>  mEffect;            // "Effect" (Default)
    std::vector<std::string> mFixtureNames;      // "Fixtures" — fixture ENTITY mIDs ("Strobe1Entity")
};
class Trigger : public nap::Resource {};         // "Name" (Required), "Bindings" (vector<EffectFixtureBinding>, Default)
class ControllerTrigger : public Trigger {};     // fired by its mapped lx::Controller
class EnterTrigger : public Trigger {};          // fired on Program load
class ExitTrigger : public Trigger {};           // fired on Program unload
}
```
Firing is service-arbitrated (`fireTrigger`/`stopTrigger`) since activation ids/claims live there. Fixture refs as entity-mID strings resolved at fire time ⇒ **no wiring replay on load**.

### 6. Controller (`module/src/controller.h/.cpp`)

`lx::Controller : Resource` (GUI label "Controller"). Props: `Name` (Required), `Trigger` (`ResourcePtr<lx::Trigger>`, Required, Default), `Mode` (`EControllerMode { Momentary, Toggle, FireOnly }`). Service semantics: Momentary — noteOn/CC≥64 fires, noteOff/CC<64 stops (edge-detected); Toggle — each on-event toggles (non-serialized `mLatched`); FireOnly — each on-event (re)fires.

### 7. MIDI binding (`module/src/midibinding.h/.cpp`)

`lx::MidiBinding : Resource` — replaces `MidiMapping`. Filter block ported verbatim from `midimapping.h/.cpp` (`Ports`/`Channels`/`Numbers` + 7 message-type bools + `matches(MidiEvent&)`), plus `Controller` (`ResourcePtr<lx::Controller>`, Required, Default). Many bindings → one controller.

### 8. Program (`module/src/program.h/.cpp`)

`lx::Program : Resource`. Props: `Name` (Required), `Triggers` (`vector<ResourcePtr<lx::Trigger>>`, Default). Load/unload semantics in the service.

### 9. Reworked `lxcontrolService` (rewrite of `module/src/lxcontrolservice.h/.cpp`; moves into `namespace lx`)

**Keeps:** `MidiHotplugMonitor` + port restart in `update()`, wildcard-`MidiInputComponentInstance` subscription via member `Slot`, MIDI log/last-event/counter Learn support, `makeUniqueID()`, createObject/init construction, `serializeObjects` → `data/user_content.json`, `loadFile` on setup.

**Service registration (new, required for Route 2):**
- `getDependentServices()` += `RTTI_OF(nap::SequenceService)` (**currently empty** — without this, ordering + `getService<SequenceService>()` aren't guaranteed).
- `registerObjectCreators(Factory&)` (new) registers `ObjectCreator<lx::ModulatorOutput, nap::SequenceService>` (the output's ctor needs a `SequenceService&`).
- `init()` calls, on `getCore().getService<nap::SequenceService>()`: `registerAdapterFactoryFunc(RTTI_OF(lx::ModulatorTrack), factory)` (lambda downcasts the `SequencePlayerOutput&` to `lx::ModulatorOutput&`, returns a `ModulatorAdapter`) **and** `registerDefaultTrackCreatorForOutput(RTTI_OF(lx::ModulatorOutput), creator)` (creator makes a `ModulatorTrack` with `mAssignedOutputID = output->mID`). Both live before any player `start()`.

**New surface:**
- Fixture registry: `registerFixture/unregisterFixture/getFixtures` (populated by `lx::FixtureComponentInstance::init`).
- Effects: `createEffect`, `addEffectParameter(lx::Effect&, TypeInfo)`, `addModulator(lx::Effect&, TypeInfo, lx::EffectParameter& target)` — mirror `createEffectLayer` leaves-first: `createObject` clock → sink `ParameterFloat` → `ModulatorOutput` (set `mParameter`, `mModulator`; push to `player->mOutputs`) → `SequencePlayer` (`mCreateEmptySequenceOnLoadFail=true`, non-existent `mSequenceFileName`, `mClock`) → `player->init()` (empty sequence → default-track-creator emits the `ModulatorTrack`) → `player->start()` (factory builds the `ModulatorAdapter`) → `setIsPlaying(true)`. For `CurveModulator`, additionally the curve output/sink/editor/GUI/track per the template. `regenerateCurveModulator`, `removeEffect`/`removeEffectParameter`/`removeModulator`.
- Triggers: `createTrigger(TypeInfo,name)`, `setTriggerBindings`, `uint64 fireTrigger(lx::Trigger&)`, `stopTrigger`, `isTriggerActive`, `removeTrigger`.
- Controllers/MIDI: `createController(name, lx::Trigger*, EControllerMode)`, `createBinding(learnedEvent, lx::Controller&)`, removes/getters.
- Programs: `createProgram`, `setProgramTriggers`, `loadProgram` (fire old ExitTriggers & stop its activations → fire new EnterTriggers → arm its ControllerTriggers), `unloadProgram`, `getActiveProgram`.

**LTP state:**
```cpp
struct Activation { uint64 mId; lx::Trigger* mTrigger; std::vector<lx::Effect*> mEffects; bool mReleasing = false; };
std::vector<Activation> mActivations;  uint64 mNextActivationId = 1;
```
`fireTrigger`: retrigger = stop-then-replace; new id; for each binding × named fixture (registry lookup by entity mID) × its channels × the effect's parameter components: on role + unit match → `channel->pushClaim(id,&param,c)`; then `effect->trigger()`. `stopTrigger`: `effect->stop()`, mark `mReleasing`; claims linger until `isFinished()`, then reaped in `update()` via `removeClaims(id)`. Sorted claim stack + `back()` = LTP + revert-on-stop for free.

**Delete-path guard:** `removeEffect`/`removeTrigger` first `stopTrigger` + reap claims for every referencing activation, then drop from tracked lists — else a "deleted" object pins a channel (underlying resource still not freed — limitation #7 — but no dangling claim).

**`update(dt)`:** hot-plug poll → dedup live activations' effects → `effect->update(dt)` (each modulator reads its own player clock / curve sink) → reap finished releasing activations.

**`onMidiEvent`:** log + learn capture → for each matching `lx::MidiBinding` → resolve controller → **only if its Trigger is in the active Program's `Triggers`** → Mode semantics → fire/stop.

**`setup()`** = `(MidiInputComponentInstance&, ResourcePtr<MidiInputPort>, ResourcePtr<RenderWindow>, ErrorState&)` — fixtures self-register; the `RenderWindow` is needed to construct `CurveModulator`s' `SequenceEditorGUI`s.

**Persistence:** `user_content.json` holds runtime-authored types (never in objects.json ⇒ blanket `getObjects<T>` safe): `lx::Effect`s + `lx::EffectParameter`s + `lx::Modulator`s. Per simple modulator, serialize its `SequencePlayer` + clock + `ModulatorOutput` + sink `ParameterFloat` (all `Default`-pointer targets, per the project `CLAUDE.md` serialization gotcha). The `ModulatorTrack` and `ModulatorAdapter` are **not** serialized — they're regenerated by the default-track-creator + factory on the next `init()`/`start()`. `CurveModulator` additionally serializes its `SequencePlayerCurveOutput`(s) + sink(s) + `SequenceEditor` + `SequenceEditorGUI`. Plus `lx::Trigger`s, `lx::Controller`s, `lx::MidiBinding`s, `lx::Program`. On load: `loadFile` (Devices auto-init/start) → populate lists via `getObjects<T>` → rematch each `CurveModulator`'s editor/GUI/outputs to its player (today's `rebuildFromLoadedContent` pattern), and rebind each simple modulator's `ModulatorOutput.mModulator` back-pointer. `CurveModulator` show files live in `data/<modID>.json`. Unloadable old file: log, rename `.bak`, continue empty. Runtime state (activations, latches, active program) not persisted — dark until a Program loads.

### 10. App (`src/lxcontrolapp.h/.cpp` — GUI rewrite, same skeleton)

`init()` resolves window/scene/camera/ParameterGUIs (fixture lookups gone — registry-driven); `update()`: input → `selectWindow` → `drawMainUI()` → `show()` any open `CurveModulator` timelines (existing pattern). Tabs **Fixtures | Effects | Triggers | Programs | MIDI**:
1. **Fixtures** — three `ParameterGUI::show()` columns (base values) + per-channel debug: resolved-value bar, claim count, claimed-vs-base marker (via `getFixtures()`).
2. **Effects** — effect list (+New/Delete); selected effect: parameter list ("Add: Dimmer | Strobe | Float | Color | Toggle") with role combo / unit checkboxes / base-value editors hand-rolled as `ImGui::SliderFloat` / `Checkbox` / `ColorEdit3(float[3])` on the raw members via `setComponentValue()` (no `ParameterGUIService` wiring for effect params — see §2); modulator list ("Add: ADSR | LFO | Step | Curve") with per-type quick controls and each modulator's `drawGui()` (PlotLines preview for ADSR/LFO/Step; `CurveModulator` shows a **Timeline** toggle opening its `SequenceEditorGUI`); clock combo (Standard/Independent) + frequency; target-param/component/Min/Max/Blend combos; live `ProgressBar(value)`; per-effect "Audition on all fixtures" toggle (transient service activation).
3. **Triggers** — list w/ type badge; create form (name + Controller/Enter/Exit combo); bindings table (Effect combo × fixture checkboxes from registry display names); **Fire/Stop**; active/releasing indicator.
4. **Programs** — list; membership checklist of Triggers; **Load/Unload**; active-program banner on every tab.
5. **MIDI** — device list + hot-plug status + live log (unchanged); Controllers section: list (name, mode combo, ControllerTrigger combo), create/delete; per-controller **Learn** (existing counter/last-event workflow → `createBinding`, message type+number only); bindings under each controller w/ delete.

### 11. Deleted (clean break)

`module/src/` (13 file pairs, **all verified present**): `fixture.*`, `fixturetype.*`, `fixturechannel.*`, `fixturechannelbinding.*`, `fixturedmxwritercomponent.*`, `effectlayer.*`, `effectlayeroverride.*`, `preset.*`, `presetmiditrigger.*`, `presetmanagercomponent.*`, `midimapping.*`, `miditriggercomponent.*` (its `EMidiTriggerAction` → `EControllerMode`), `midiparametercomponent.*`. Also `data/user_content.json` (+ stale `data/presets/`). `midihotplugmonitor.*` stays; `lxcontrolservice.*` rewritten (into `namespace lx`).

**Manifests:** `napsequence` + `napsequencegui` + `napimgui` **stay required** in `module/module.json` (every modulator owns a player; `CurveModulator` uses the editor GUI); `app.json` unchanged. `regenerate.bat` whenever `.cpp` files are added/removed.

---

## Implementation phases

Every phase ends buildable + runnable. Loop: (files added/removed? → `regenerate.bat`) → `build.bat` with **Napkin closed** → run → read the ResourceManager load log → observe. Validate `objects.json` edits with `nap-validate-data-json` first. Use `nap-adding-a-module-class` per new class and `nap-resource-graph`/`nap-artnet-dmx`/`nap-midi`/`nap-parameters`/`nap-sequence` when touching those areas.

**Phase 1 — Fixture ECS + demolition.** *(Irreducibly atomic.)* Build inside-out: add `channelrole.*`, `fixturechannelmapping.*`, `fixturechannelcomponent.*`, `fixturecomponent.*` (all `namespace lx`), compile → `nap-validate-data-json` on the rewritten `objects.json` (§1) → strip `lxcontrolservice.*` to hot-plug + MIDI subscription/log/learn + fixture registry + save/load stubs → reduce app tabs to Fixtures + MIDI. Delete the 13 pairs (§11) + `user_content.json` in one commit.
*Verify:* clean load log; Fixtures sliders drive Art-Net; MIDI log streams; hot-plug reconnects.

**Phase 2 — Effects + Parameters + Modulators + custom Output/Adapter + persistence v1.** Add `effectparameter.*`, `modulatortrack.*`, `modulatoroutput.*`, `modulatoradapter.*`, `modulator.*`, `adsrmodulator.*`, `lfomodulator.*`, `stepmodulator.*`, `curvemodulator.*`, `effect.*`. Service: add `SequenceService` dependency + `registerObjectCreators` + the adapter-factory/default-track-creator registrations in `init()`; effect/param CRUD; `addModulator` (per-modulator player+clock+output; `createEffectLayer`-style graph for `CurveModulator`); effect update loop; audition; save/rebuild. **First checkpoint after the three custom classes + registration: build, create one LFO, and log `player->getSequenceConst().mTracks` right after `init()` to confirm the `ModulatorTrack` was auto-created (the default-track-creator chain is inferred from doc-comments — verify before building on it).** App: Effects tab incl. per-modulator `drawGui()` (PlotLines + Curve timeline toggle) and clock combo.
*Verify:* create ADSR/LFO/Step (custom output/adapter → sink param, PlotLines previews) and a CurveModulator (editable timeline); audition drives real DMX (LFO pulses a dimmer; ADSR sustain holds exactly while held, releases on stop); Independent-clock ADSR feels smooth; restart → all modulators reload and play.

**Phase 3 — Triggers + full LTP.** Add `trigger.*`. Service: activations, fire/stop, claim push/reap, release-linger, delete-path guard. App: Triggers tab w/ Fire/Stop.
*Verify:* fire A (LFO on Strobe1) then B (ADSR on Strobe1) → B wins; stop B → A resumes; stop A → base; ADSR release rings out; delete an effect mid-activation → channel reverts; save/reload round-trips `lx::EffectFixtureBinding` (first vector-of-struct — inspect JSON).

**Phase 4 — Controllers + MIDI bindings + Learn.** Add `controller.*`, `midibinding.*`. Service: `onMidiEvent` dispatch (arm unconditionally until Phase 5), CRUD + persistence. App: MIDI tab rebuild.
*Verify (real device or loopMIDI):* learn a note onto Momentary → on fires / off stops; Toggle latches; two bindings → one controller; persists across restart.

**Phase 5 — Programs.** Add `program.*`. Service: load/unload, Enter/Exit firing, program-scoped controller arming, final save/rebuild. App: Programs tab + banner.
*Verify:* EnterTrigger lights on Load; ExitTrigger on switch; ControllerTriggers respond only while their program is loaded; cold-start → reload all, load program, play a full show.

## Verification (end-to-end, after Phase 5)

1. Fresh start, no `user_content.json`: author 2 effects (color LFO, dimmer ADSR), 1 CurveModulator, 3 triggers (Controller/Enter/Exit), 1 controller learned from a MIDI pad, 1 program.
2. Restart: all content reloads; load the program: EnterTrigger fires; pad press/release drives the ADSR with visible release-linger; simultaneous triggers show LTP order + revert.
3. Watch DMX output and the Fixtures-tab claim debug for every step.

## Risks / open questions

1. **Default-track-creator chain is inferred, not header-proven** — that `player->init()` with `mCreateEmptySequenceOnLoadFail=true` → `createDefaultSequence(mOutputs)` → the registered `registerDefaultTrackCreatorForOutput` creator emits a `ModulatorTrack` is read from doc-comments (`sequenceservice.h:58-77`); bodies are in `napsequence.dll`. It's the same mechanism the current curve effect layers rely on, but **Phase 2's first checkpoint is to log `getSequenceConst().mTracks` after `init()` to confirm.** Fallback: author a one-track show file per modulator. **Primary review flag.**
2. **Thread-hop correctness** — `evaluate()` runs in `adapter.tick()` on the clock thread; the value must reach the sink `ParameterFloat` only via `ModulatorOutput::update()` on the main thread (mutex-guarded), never written off-thread — else it races the GUI and the DMX write. Mirror the curve adapter's `mUseMainThread` path exactly; do not replicate its direct-set branch.
3. **Per-modulator `IndependentClock` = one thread each** — prefer `Standard` unless jitter matters; many Independent-clock modulators = many threads.
4. **`vector<struct{ResourcePtr,…}>` serialization** (`lx::EffectFixtureBinding`) — SDK precedent (`Scene::RootEntity`) but literal macro unread; Phase-3 round-trip is the gate; fallback: promote to a standalone Resource.
5. **Shared `lx::Effect` state across concurrent Triggers** — two triggers on one effect share modulator state; re-fire retriggers both. Accepted v1; per-activation effect instances if it bites.
6. **CurveModulator regen vs hand edits** — only `CurveModulator` is hand-authored, so there's no generator-vs-edit conflict; changing its Min/Max/duration via quick controls must go through the `SequenceController` to stay thread-safe.
7. **CC-driven Momentary jitter** — threshold-crossing edge detection.
8. **No true resource deletion** (`ResourceManager` has no public remove) — deleted content lingers until restart, excluded from save; the delete-path guard prevents the stale-claim symptom. Unchanged limitation.
9. **16-bit widths** — enum kept, only Value8 emitted (as today).
10. **objects.json duplication** — 3 × ~22 channel blocks, no shared FixtureType; accepted for real ECS channels (future generator script possible).
11. **Generic names now namespaced** — `lx::Effect/Trigger/Program` can't collide with `nap::` types; the `lx` namespace removes the earlier prefix concern.
