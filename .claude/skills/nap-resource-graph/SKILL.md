---
name: nap-resource-graph
description: Use when composing, editing, or debugging a NAP app's resource/entity/component graph in its data json (objects.json) — adding an Entity, Component, or Resource, wiring references between objects, setting up a Scene, or diagnosing load errors like "Unknown object type" or "Encountered embedded pointer that points to a non-embedded object".
---

# NAP resource / entity / component graph

## Overview

A NAP app's behavior is composed declaratively in a data json (commonly `objects.json`): a graph of
**Resources** deserialized at startup by the `ResourceManager`. The core idea is the
**resource ↔ instance split** — JSON describes static *blueprints* (resources); at runtime NAP
creates a matching `*Instance` object for the scene-graph types. **JSON only ever names resource
types.** Get this split and the JSON key names right and most of the model follows.

## When to use

- Adding/removing an Entity, Component, or standalone Resource in a data json.
- Wiring one object to another (by id, or inline).
- Setting up or editing a `nap::Scene`.
- Debugging load failures: `Unknown object type <X>`, `Encountered embedded pointer that points to a
  non-embedded object`, unresolved id references.

**Not for:** authoring a *new* C++ resource/component class (that's a module-code task) or runtime
construction via `ResourceManager::createObject<T>()`. This skill is the JSON graph.

## Mental model

- `rtti::Object` (owns `mID`) → `Resource` → everything authorable in JSON. (`<SDK>/include/nap/resource.h`)
- **Entity** (`nap::Entity`) is a *container*: a list of Components and child Entities. Its runtime
  form is `EntityInstance`.
- **Component** is a `Resource` (static data from JSON); its runtime counterpart is a
  `ComponentInstance`. The two halves are bound by the `DECLARE_COMPONENT(Comp, CompInstance)` macro
  in the class header. (`<SDK>/system_modules/napscene/include/component.h`)
- **You author the resource side only.** JSON `"Type"` is always the resource class
  (`nap::TransformComponent`), never the instance class.

## objects.json anatomy

Top-level is one object with an `"Objects"` array. Every entry has `"Type"` (the fully-qualified
RTTI-registered class name — exactly the `RTTI_BEGIN_CLASS` argument) and `"mID"` (unique id).
Entities hold components inline under `"Components"` and children under `"Children"`.

```json
{
    "Objects": [
        {
            "Type": "nap::Entity",
            "mID": "World",
            "Components": [
                { "Type": "nap::TransformComponent", "mID": "WorldXform",
                  "Properties": { "Translate": { "x": 0.0, "y": 0.0, "z": 0.0 } } },
                { "Type": "nap::RenderableMeshComponent", "mID": "WorldRender",
                  "Mesh": "WorldMesh", "MaterialInstance": { "Material": "WorldMaterial" } }
            ],
            "Children": []
        }
    ]
}
```

Standalone resources (meshes, materials, windows, textures, fonts) sit as siblings in the same
`"Objects"` array. `nap::ResourceGroup` (`"Members"` + `"Children"`) is organizational grouping only —
not an entity.

You need not set every property: a property registered `Default` falls back to the class default when
omitted; only `Required` properties must be present (`EPropertyMetaData` in
`<SDK>/include/rtti/typeinfo.h`). A copied working block already reflects this. Note a graph that
*loads* is not the same as a graph that *does something* — a runnable app also needs the supporting
resources (a `RenderWindow`, a `Scene`, cameras) and its render path wired up; use a full demo as the
template for a complete app, not just a single component block.

## Scenes: how entities come alive

Entities in `"Objects"` are just blueprints. A `nap::Scene` **spawns** them. The scene's serialized
key is `"Entities"` (the C++ member is `mEntities`, a list of `RootEntity`); each element is
`{ "Entity": "<entity-mID>", "InstanceProperties": [] }`.

```json
{ "Type": "nap::Scene", "mID": "Scene",
  "Entities": [
      { "Entity": "World",             "InstanceProperties": [] },
      { "Entity": "PerspectiveCamera", "InstanceProperties": [] }
  ]
}
```

Code then fetches instances: `scene->findEntity("World")`; non-entity resources via
`resourceManager->findObject<T>("mID")`. (`<SDK>/system_modules/napscene/include/scene.h`)

## References between objects: Default vs Embedded

The #1 source of load errors. A pointer property is one of two kinds, and the JSON form differs:
- **Default** (common) — a **string id reference**: `"Prop": "OtherMID"`, pointing at a separately
  declared object.
- **Embedded** — a **full inline object**: `"Prop": { "Type": "...", ... }`, defined only there.

The kind is fixed in C++ RTTI and is **not** visible in the JSON, the header, or the docs page. Get
the correct shape by copying a working block (`nap_usage.py <Type>`); a mismatch fails with
`Encountered embedded pointer that points to a non-embedded object` (which doesn't name the property).
Cross-entity refs use `nap::ComponentPtr` path syntax (`RootEntity/Comp`, `./`, `../`). Full detail,
the headers-only reasoning, and the fix → **`references/lookup-and-pointers.md`**.

## Finding the exact Type, its properties, and each pointer's kind

Do **not** guess a `"Type"`, property, or pointer shape. Because system modules ship **headers-only**,
the reliable order is:
1. **Copy a working block (primary)** — `python ../nap-skill-authoring/scripts/nap_usage.py <Type>`
   gives correct names + Embedded/Default shape + enum strings at once, and it provably loads.
2. **More properties / inherited ones** → headers, walking the `RTTI_ENABLE(Base)` chain.
3. **Spelling / docs URL / owning module** → `nap_doc.py <Class>` and `nap_usage.py --module <Class>`.

Full procedure → **`references/lookup-and-pointers.md`**; lookup mechanics →
`nap-skill-authoring/references/authoritative-sources.md`.

## Verify the graph actually loads

Editing the JSON is not done until you've seen it load. NAP validates the whole graph at startup, so
the test is: **build (if you touched C++) and run the app, then read the `ResourceManager` load log.**
- A clean load prints no errors and the app window appears. A bad graph aborts startup with the exact
  message (`Unknown object type…`, `Encountered embedded pointer…`, unresolved id) — that log line is
  your diagnosis.
- JSON-only edits need no rebuild (the data json is read at runtime). Adding or changing an
  **app-authored C++ type** needs `build.bat` — and `regenerate.bat` first if you added a new `.cpp`,
  and Napkin closed (it locks the module DLL). See the project `CLAUDE.md` for these gotchas.
- Declaring an entity is not enough for it to appear — confirm it's in a `Scene`'s `"Entities"` list,
  and (for a renderable) that the app's render path actually draws it.

## Common mistakes

- **`Unknown object type <X>`** — the `"Type"` string is misspelled, not fully qualified, or its
  module isn't a `RequiredModule`. Verify the exact registered name (resolver / grep `RTTI_BEGIN_CLASS`).
  For an **app-authored** type, this also appears when a newly added `.cpp` wasn't compiled in because
  `regenerate.bat` wasn't run after adding it — see the project `CLAUDE.md`.
- **Naming the instance class in JSON** — use `nap::FooComponent`, never `nap::FooComponentInstance`.
- **Embedded/Default mismatch** — see above; check the `RTTI_PROPERTY` flag.
- **Declared an entity but nothing happens** — it isn't in a `Scene`'s `"Entities"` list, so it was
  never spawned.
- **Wrong key casing** — the keys are exactly `Objects`, `Type`, `mID`, `Components`, `Children`, and
  (in a Scene) `Entities`.

## Worked examples

See `references/worked-examples.md` for concept → exact demo `file:line` (minimal graph + scene,
a custom component, an Embedded pointer, and lxcontrol's own graph as a marked in-repo example).
