# Resource-graph worked examples (verified against this SDK, 0.8.0)

`<SDK>` = the SDK root (this app is at `<SDK>/apps/lxcontrol`). Line numbers verified on disk; if
they drift after an SDK update, grep the quoted anchor.

To find *more* working JSON for any type than the curated set below, run
`python ../../nap-skill-authoring/scripts/nap_usage.py <Type>` — it prints every demo/app block using
that `Type`, which is the fastest way to copy a correct, load-tested block.

## 1. Minimal graph + Scene (built-in types only)

`<SDK>/demos/helloworld/data/helloworld.json` — the cleanest ground truth for JSON shape. Uses only
system-module types (no custom module).

- Top-level `"Objects"` array; entities are `"Type": "nap::Entity"` (`:4`, `:56`, `:124`, `:177`).
- The `World` entity (`:177`–`:178`) holds `TransformComponent`, `RenderableMeshComponent`, and
  `RotateComponent` inline under `"Components"`.
- The `nap::Scene` (`:376`–`:377`) spawns four root entities via `"Entities": [ { "Entity": "...",
  "InstanceProperties": [] }, ... ]`.

Matching C++ — `<SDK>/demos/helloworld/src/helloworldapp.cpp`:
- Non-entity resources: `mResourceManager->findObject<nap::RenderWindow>("Window0")` (`:43`),
  `findObject<Scene>("Scene")` (`:47`).
- Entities: `scene->findEntity("World")` (`:50`–`:55`).

## 2. A custom Component authored in a demo's own module

`<SDK>/demos/curveball/module/src/animationcomponent.h` + `.cpp` — the resource↔instance split in one
small component (`nap::AnimatorComponent`, one `ResourcePtr` property).

- Header: `class AnimatorComponent : public Component` (`:22`) with
  `DECLARE_COMPONENT(AnimatorComponent, AnimatorComponentInstance)` (`:25`); the instance class
  `class AnimatorComponentInstance : public ComponentInstance` (`:44`).
- `.cpp`: `RTTI_BEGIN_CLASS(nap::AnimatorComponent, ...)` (`:14`) — **the first argument is exactly the
  JSON `"Type"` string**. The instance registers with
  `RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::AnimatorComponentInstance)` (`:18`) +
  `RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)` (`:19`).
- Usage in JSON: `<SDK>/demos/curveball/data/curveball.json`.

## 3. Embedded vs Default pointer

`<SDK>/demos/linemorphing/module/src/linenoisecomponent.cpp` — the one demo that registers an
**Embedded** pointer:
- `RTTI_BEGIN_CLASS(nap::LineNoiseComponent, ...)` (`:24`).
- `RTTI_PROPERTY("Properties", &...::mProperties, EPropertyMetaData::Required | EPropertyMetaData::Embedded)`
  (`:25`) → in JSON `"Properties"` is a **full inline object**, not an id string.
- The sibling `"BlendComponent"` property is registered without `Embedded` (Default) → in JSON it's an
  **id string reference**.

The flags themselves: `<SDK>/include/rtti/typeinfo.h` — `enum class EPropertyMetaData` (`:165`),
`Embedded = 4` (`:170`). Default (a plain id-string reference) is the value `0`.

## 4. The macro that binds resource ↔ instance

`<SDK>/system_modules/napscene/include/component.h` — `#define DECLARE_COMPONENT(ComponentType,
ComponentInstanceType)` (`:197`). This is why every component has two classes and why JSON only names
the resource one.

Scene / entity runtime API: `<SDK>/system_modules/napscene/include/scene.h`
(`Scene::init` spawns entities; `Scene::findEntity`; `RootEntity`, `mEntities`).

## 5. lxcontrol's own graph (in-repo example — not the definition of NAP)

`apps/lxcontrol/data/objects.json` follows the identical shape at larger scale; its `nap::Scene` is at
`:1507`–`:1508` (root entities incl. `FixtureDriverEntity`, `MidiMonitorEntity`). App-authored
components live in `apps/lxcontrol/module/src` — e.g.
`fixturedmxwritercomponent.cpp`: `RTTI_BEGIN_CLASS(nap::FixtureDmxWriterComponent)` (`:5`) +
`RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::FixtureDmxWriterComponentInstance)` (`:9`). These
app-specific `"Type"`s are **not** in the online reference — grep the module source.
