# NAP authoritative sources & how to look them up

The single reference every NAP skill points to for grounding API claims. Do not restate this
in other skills — link here.

## First, the fact that changes everything: system modules are headers-only

The SDK ships system modules (`naprender`, `napscene`, `napmidi`, …) as **headers + prebuilt
libraries**. There are **no `.cpp` files on disk** for them (`find system_modules -name '*.cpp'`
returns 0). RTTI registration — `RTTI_PROPERTY` (which carries the Embedded/Default flag) and
`RTTI_ENUM_VALUE` (the serialized enum strings) — lives in those unshipped `.cpp` files.

Consequence: for the types you compose most (`Material`, `RenderableMeshComponent`, `Scene`, …)
**some facts are simply not on disk and not on the docs site.** Know which source has which fact:

| Fact you need for correct JSON | Local header | Docs class page | A demo/app JSON that already uses the type |
|---|---|---|---|
| Property **names** | ✅ `///< Property: 'X'` — **but walk the base chain** | ✅ member table + inheritance | ⚠️ only the ones that block sets |
| Property C++ type (hints pointer vs value) | ✅ | ✅ | — |
| **Embedded vs Default** shape (inline object vs id string) | ❌ (in unshipped `.cpp`) | ❌ | ✅ the block shows it, and it provably loads |
| Enum **serialized string** (e.g. `"Opaque"`) | ❌ (bare `enum class`) | ❌ (C++ identifier only) | ✅ exact string |
| Which **module** declares the type | ✅ (header path) | ✅ | — |

**The takeaway:** the Embedded/Default shape and enum strings for system-module types come **only**
from a working JSON block. That is why the copy-and-adapt technique below is the primary method, not
a convenience. (For **app/demo-authored** types the `.cpp` *is* on disk — grep its `RTTI_PROPERTY`
directly.)

## PRIMARY technique: copy a working JSON block, then adapt it

The most reliable way to produce correct NAP JSON is to find a demo (or another app) whose data json
**already uses the type** and adapt that block. It carries the exact property names, the
Embedded/Default shape, and the exact enum strings at once — and it provably loads.

```bash
python scripts/nap_usage.py nap::RotateComponent   # every demo/app JSON object of this Type
python scripts/nap_usage.py --all nap::Material     # all matches (default shows the first few)
python scripts/nap_usage.py --module RotateComponent # header + owning module (fixes wrong-module guesses)
```

Then adjust `mID`s and values to your case. Only reach for headers/docs to (a) discover properties a
demo didn't set, or (b) confirm a type when no demo uses it.

## The source ladder (most authoritative first)

| # | Source | Best for | Trust |
|---|--------|---------|-------|
| 1 | **A working JSON block** — `nap_usage.py`, `<SDK>/demos/*/data`, `<SDK>/apps/*/data` | Correct shape/flags/enum strings for a type, provably loading. | Ground truth for JSON |
| 2 | **Local SDK headers** — `<SDK>/include`, `<SDK>/system_modules/*/include` | Property **names** (walk the base chain), C++ types, owning module. | Ground truth for this SDK's API surface |
| 3 | **App/module source** — `<SDK>/apps/*/module/src` (has `.cpp`) | App-authored types: exact `RTTI_PROPERTY` incl. the Embedded/Default flag. | Ground truth for app types |
| 4 | **Class / namespace reference** — `docs.nap-framework.tech` | Browsing API, inheritance, doc comments; types not in the app's modules. | Authoritative; may be a different version than disk |
| 5 | **Manual pages** — `docs.nap-framework.tech/pages.html` | Concepts/architecture (Resource Management, Scene, Rendering, Sequencer, Audio). | Authoritative concepts |
| 6 | **Project gotchas** — the app's `CLAUDE.md` | Hard-won, non-obvious rules (regenerate-after-new-`.cpp`, Napkin DLL lock, Embedded vs Default, serialization pitfalls). | Ground truth for this project |
| 7 | **Community modules** — `modules.nap-framework.tech` | Third-party modules (napfft, naplua, naprest, …) with GitHub links. | Community; quality varies |

`<SDK>` = the SDK root; this app is at `<SDK>/apps/lxcontrol`, so from the app dir headers are
`../../include`, system modules `../../system_modules`, demos `../../demos`. When the docs and a local
header disagree, the header wins — you build against the SDK on disk.

## Enumerating a type's FULL property set (walk the inheritance chain)

Property names are in headers as `///< Property: 'Name'`, but a class only declares its *own*
properties — inherited ones live in base classes named by `RTTI_ENABLE(Base)`. Grepping one header
misses them. Walk up:

```bash
# leaf class — declares Mesh / MaterialInstance / ClipRect / LineWidth, and RTTI_ENABLE(RenderableComponent)
grep -n "RTTI_ENABLE\|Property:" ../../system_modules/naprender/include/renderablemeshcomponent.h
# base class — adds Visible / Layer / Tags, and RTTI_ENABLE(Component)
grep -n "RTTI_ENABLE\|Property:" ../../system_modules/naprender/include/rendercomponent.h
```

Repeat until `RTTI_ENABLE(Resource)`/`Object`. The docs class page shows the same via its inheritance
list and member table.

## Enum values

The serialized string is **not** in the header (it's `RTTI_ENUM_VALUE` in unshipped `.cpp`). To get it:
1. `nap_usage.py <TypeThatHasTheEnumProperty>` and read the value in a working block (authoritative), or
2. read the C++ `enum class` in the header for the identifier list (e.g. `EBlendMode` in
   `materialcommon.h`) — the string usually equals the identifier, but **confirm against a working
   block** before trusting it.

## Finding a docs URL: the hashed-URL problem

Doxygen URLs carry a **non-derivable hash prefix**: `nap::Material` →
`https://docs.nap-framework.tech/d2/de3/classnap_1_1_material`. You cannot build this from the name,
and a summarizing fetch tool (e.g. WebFetch) will hand back **fabricated** hashes. Resolve names with:

```bash
python scripts/nap_doc.py Material                 # class by short name
python scripts/nap_doc.py nap::audio::AudioService # fully-qualified (disambiguates)
python scripts/nap_doc.py --ns nap::audio          # namespace
python scripts/nap_doc.py --page rendering         # manual page (title substring)
python scripts/nap_doc.py --list Sequence          # class names containing a substring
python scripts/nap_doc.py --refresh Scene          # re-pull the indexes first
python scripts/nap_doc.py --selftest               # verify the index still parses
```

A prebuilt index ships at `scripts/.doc_cache/index.json`, so lookups work **offline on a fresh
checkout**; `--refresh` rebuilds it after a docs-site update. The script refuses to cache a broken
parse (if the Doxygen layout changes it errors loudly rather than returning silent false "no match"es
— run `--selftest` if results look empty).

### Manual fallback (no python)

Fetch `https://docs.nap-framework.tech/classes.html` (or `/namespaces.html`, `/pages.html`) and find
the anchor. Note the **single quotes** and **no `.html`** extension:

```html
<a class='el' href='/d2/de3/classnap_1_1_material'>Material</a> (<a class='el' href='/d3/d14/namespacenap'>nap</a>)
```

URL = `https://docs.nap-framework.tech` + the `href`; the namespace is the second link in the row. If
the fetch returns a login/challenge page or a summarizer that may drop hrefs, treat the result as
unverified and prefer the script or a local header/demo.

## When a type isn't in the resolver / class index

"No match" does not mean invalid. Two common cases — fall back to grepping local headers/JSON:

- **App/module-authored types** (e.g. lxcontrol's `nap::FixtureDmxWriterComponent`) live in
  `apps/<app>/module/src`, not the SDK. `nap_usage.py --module <Type>` finds the header.
- **Template-alias types.** Doxygen indexes the template, not the alias. `nap::ResourceGroup` is a
  valid `"Type"` but is `using ResourceGroup = Group<Resource>` at `<SDK>/include/nap/group.h:152`
  (registered via `DEFINE_GROUP`); the resolver won't have the alias. Grep the header.
