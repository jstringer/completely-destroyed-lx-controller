---
name: nap-adding-a-module-class
description: Use when authoring a NEW C++ class in a NAP module — a Component, Resource, or Service that will be referenced from a data json — including writing its RTTI registration (RTTI_BEGIN_CLASS/RTTI_PROPERTY/DECLARE_COMPONENT), the resource↔instance split, and the regenerate/build steps. Not for composing existing types in JSON (that's nap-resource-graph).
---

# Adding a new NAP module class

## Overview

A C++ class becomes usable from a data json only when it is **RTTI-registered** — the
`RTTI_BEGIN_CLASS(nap::X, …)` argument is the exact `"Type"` string the JSON uses. Two shapes matter:
- A **Resource** is one class (`: public Resource`) with an `init()`.
- A **Component** is **two** classes — a `Component` (the JSON-authored resource) and a
  `ComponentInstance` (its runtime counterpart) — bound by `DECLARE_COMPONENT(Comp, CompInstance)`.
  JSON only ever names the resource class.

Hand-writing the RTTI boilerplate is where subtle errors creep in, so scaffold it.

## When to use

- Adding a new Component / Resource / channel-type / mapping class to a module's `src/`.
- Writing or fixing a class's RTTI registration.

**Not for:** composing existing types in a data json → `nap-resource-graph`. Wiring runtime-built
objects via `ResourceManager::createObject<T>()` has its own manual init/start ordering — see the
project `CLAUDE.md` and (later) a sequence/runtime-construction skill.

## Workflow

1. **Scaffold** the class with the generator — it emits correct RTTI, the resource/instance split,
   and resolves pointer-target `#include`s:
   ```bash
   python ../nap-skill-authoring/scripts/nap_new_component.py --name SpinComponent --module <app> \
       --desc "spins a transform" --prop Speed:float --ptr Curve:nap::math::FloatFCurve
   # --kind resource for a plain Resource; --embed Name:nap::T for an Embedded pointer; --dry-run to preview
   ```
2. **Fill in logic** — the resource's members are done; write the instance's `init()` (fetch resource
   props via `getComponent<T>()`, sibling instances via `getEntityInstance()->findComponent<T>()`) and
   `update()`. Pick each property's flag deliberately (see below).
3. **`regenerate.bat`** — **mandatory after adding any new `.cpp`.** The VS project's file list is
   captured at regenerate time; skip this and `build.bat` reports success while never compiling your
   class, and the app fails to load with `Unknown object type nap::YourClass`.
4. **`build.bat`** — with **Napkin closed** (it locks the module DLL → `LNK1104`).
5. **Verify** — `nap-build-run-verify` for the build/run loop, `nap-validate-data-json` once you use
   the new `"Type"` in a json.

## Choosing a property's flag (the load-error trap)

`RTTI_PROPERTY`'s flag decides the JSON form: `Default` (an `mID` **string reference**), `Required`
(must be present), or `Embedded` (a **full inline object**, defined only there). A pointer's flag and
the JSON must agree or the load fails with `Encountered embedded pointer that points to a non-embedded
object`. Full flag table and every RTTI macro: `references/rtti-macros.md`.

## Common mistakes

- **New `.cpp`, no `regenerate.bat`** → `Unknown object type` despite correct code. The #1 gotcha.
- **Registering/naming the instance class in JSON** — JSON uses `nap::SpinComponent`, never
  `nap::SpinComponentInstance`.
- **Forgetting the instance's RTTI** — a Component needs both `RTTI_BEGIN_CLASS(...)` and
  `RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::…Instance)` + `RTTI_CONSTRUCTOR(nap::EntityInstance&,
  nap::Component&)`. The scaffolder emits both.
- **Using `"Type"` as a custom property name** — reserved; NAP uses it for the class name.
- **Property member with no matching `RTTI_PROPERTY`** (or vice-versa) — the two must line up.
