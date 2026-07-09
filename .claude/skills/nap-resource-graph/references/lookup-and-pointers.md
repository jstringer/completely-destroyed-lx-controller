# Finding a Type's exact API, and Default vs Embedded pointers

Detail for `nap-resource-graph`'s two hardest sub-tasks. The lookup mechanics themselves (scripts,
source ladder) live in `nap-skill-authoring/references/authoritative-sources.md` — this file is the
resource-graph-specific application of them.

## Finding the exact Type, its properties, and each pointer's kind

Do **not** guess a `"Type"` string, property name, or pointer shape. NAP's system modules ship as
**headers only** (no `.cpp`), so the Embedded/Default shape and enum strings are not on disk for them —
which makes copying a working block the most reliable path, not merely the fastest.

1. **Copy a working block (primary).** `python ../nap-skill-authoring/scripts/nap_usage.py <Type>`
   prints every demo/app JSON object of that Type. Adapt one: it already has the correct property
   names, the inline-vs-id-string (Embedded/Default) shape, and exact enum strings, and it provably
   loads.
2. **Discover more properties** a demo didn't set → read the header's `///< Property:` comments **and
   walk the `RTTI_ENABLE(Base)` chain** for inherited ones. A leaf class omits base properties: e.g.
   `renderablemeshcomponent.h` declares only `Mesh`/`MaterialInstance`/`ClipRect`/`LineWidth`, while
   its base `rendercomponent.h` adds `Visible`/`Layer`/`Tags`. The docs class page's member table and
   inheritance list show the same.
3. **Confirm a Type exists / its spelling / its module** → `nap_doc.py <Class>` (docs URL) and
   `nap_usage.py --module <Class>` (declaring header + owning module — don't guess the module, and
   check it's a `RequiredModule` in `app.json`/`module.json`).
4. **App/demo-authored types** (e.g. lxcontrol's) have `.cpp` on disk — grep its `RTTI_PROPERTY` for
   the exact flag.

## Default vs Embedded pointers (the #1 source of load errors)

A pointer property is registered in C++ RTTI as one of:

| Kind | JSON form | Meaning |
|------|-----------|---------|
| **Default** (the common case) | `"Prop": "OtherMID"` — a **string id reference** | Points at a separately-declared object by its `mID`. |
| **Embedded** | `"Prop": { "Type": "...", ... }` — a **full inline object** | The target is defined inline and *only* here (declaring it also as a top-level object is an error). |

`EPropertyMetaData` flags live in `<SDK>/include/rtti/typeinfo.h` (`Default = 0`, `Required = 1`,
`Embedded = 4`). The kind is fixed in the class's `RTTI_PROPERTY` registration (in `.cpp`) — it is
**not** visible in the JSON, the header field type, or the docs page.

- **System-module types:** no `.cpp` on disk, so you **cannot read the flag anywhere on disk**. The
  only reliable source is a working JSON block (`nap_usage.py <Type>`) — it shows inline-object vs
  id-string, and it provably loads.
- **App/demo-authored types:** grep the `RTTI_PROPERTY` line in the module's `.cpp`
  (`RTTI_PROPERTY("Prop", &Class::mProp, ... EPropertyMetaData::Embedded ...)`).

**The error:** writing an inline object where `Default` is expected, or an id string where `Embedded`
is expected, fails with `Encountered embedded pointer that points to a non-embedded object` — and the
message does **not** name the offending property. Fix by copying the pointer's shape from a working
block, or (app types) checking the `RTTI_PROPERTY` flag.

**Cross-entity component references** use `nap::ComponentPtr<T>` with a `/`-separated path:
`RootEntity/ComponentID` (absolute), `./Child/Comp` (relative to this entity),
`../Sibling/Comp` (via parent). (`<SDK>/system_modules/napscene/include/componentptr.h`)
