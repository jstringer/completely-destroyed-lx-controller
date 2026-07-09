---
name: nap-parameters
description: Use when working with NAP's napparameter/napparametergui modules — declaring ParameterFloat/Int/Bool/Vec/RGBColor/Enum etc. in a data json, nesting them in a ParameterGroup, showing a ParameterGUI, or saving/loading presets via ParameterService. Covers the parameter type family, the Embedded ParameterGroup, and preset storage.
---

# NAP parameters (napparameter + napparametergui)

## Overview

A `Parameter` is a serializable, GUI-editable, reference-able value (`nap::ParameterFloat`,
`ParameterRGBColorFloat`, …). Parameters live in a `ParameterGroup` (which nests child groups); a
`ParameterGUI` renders a group and drives preset save/load through `ParameterService`. Parameters are
the common binding target for MIDI, sequences, and component inputs.

## When to use

- Declaring parameters + a `ParameterGroup` in a data json.
- Showing a `ParameterGUI`, or saving/loading presets.
- Choosing the right `Parameter*` type, or debugging an "Encountered embedded pointer" error on a
  group.

## Mental model

- Each `Parameter*` is a `Resource` with an `mID` (for references) **and** a display-only `Name`.
- Value shape depends on the family: **numeric** (`Value`/`Minimum`/`Maximum`, always clamps),
  **simple** (`Value` only — bool/string/color/quat/mat), **vec** (`Value {x,y,…}` + opt-in `Clamp`),
  **enum** (needs manual RTTI registration). See the type table in `references/parameter-types.md`.
- **`ParameterGroup` is a `Group<Parameter>`**: it holds parameters under `"Parameters"` and child
  groups under `"Groups"` (renamed from the generic Group's `Members`/`Children`), and **both are
  `Embedded`** — written as full inline objects, not id references.
- Embedded parameters still register their `mID`, so other objects (components, sequence outputs, MIDI
  mappings) reference them by that id normally.
- A `ParameterGUI` points at one group via `Group` (a **Default** id reference) and is shown every
  frame from `update()`.

## JSON shape

Copy a working group: `python ../nap-skill-authoring/scripts/nap_usage.py nap::ParameterGroup` (or a
specific `nap::ParameterFloat`, `nap::ParameterRGBColorFloat`, …). Full per-type blocks and the group
structure are in **`references/parameter-types.md`**.

## Showing a ParameterGUI (C++)

In `update()`, select the window, then embed the parameters into your own ImGui window with
`show(false)`:

```cpp
mGuiService->selectWindow(mRenderWindow);
ImGui::Begin("Controls");
mParameterGUI->show(false);   // false = embed into the active window; true (default) = its own window
ImGui::End();
```

Presets go through `ParameterService` (`savePreset`/`loadPreset`/`getPresets(group, …)`), stored at
`<PresetsDirectory>/<groupID>/<file>.json` (`PresetsDirectory` defaults to `"Presets"`). The GUI's
save/load popup drives these for you.

## Common mistakes

- **Putting an id string in `Parameters`/`Groups`** → `Encountered embedded pointer that points to a
  non-embedded object`. They are `Embedded` — write full inline objects. (Conversely `ParameterGUI.Group`
  *is* a plain id reference.)
- **Using `Members`/`Children`** in a ParameterGroup — it uses `Parameters`/`Groups`.
- **Confusing `Name` and `mID`** — `mID` is the reference/preset key; `Name` is the display label
  (falls back to `mID` if empty). Both appear in the json.
- **Value shape mismatch** — color `Value` is `{ "Values": [r,g,b] }`; vec `Value` is `{ "x","y","z" }`.
  Numeric parameters always clamp to `[Minimum,Maximum]`; vec parameters clamp only if `Clamp: true`.
- **`ParameterEnum` without RTTI** — unlike the numeric/vec/color families there's no built-in
  typedef; you must `RTTI_BEGIN_ENUM` the enum and `DEFINE_ENUM_PARAMETER` the typedef
  (`nap-adding-a-module-class`), or the `Type` won't resolve.
- **`show()` ordering** — must be called every frame in `update()`, after `selectWindow`; `show(true)`
  spawns a standalone window (usually you want `false`).

## Verify

`nap-validate-data-json`, then `nap-build-run-verify`. Worked examples:
`<SDK>/demos/computeflocking` (nested groups + many parameter types + id-reference wiring),
`<SDK>/demos/linemorphing` (cleanest `selectWindow → Begin → show(false) → End` usage). Details:
**`references/parameter-types.md`**.
