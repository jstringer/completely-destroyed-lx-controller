---
name: nap-validate-data-json
description: Use right after editing or writing a NAP data json (objects.json) and before running the app, to catch load errors statically — unknown "Type" strings, duplicate mIDs, dangling id references, Embedded/Default shape mismatches, and entities that are never spawned by a Scene. Also use when triaging a NAP startup/load failure.
---

# Validate a NAP data json before running

## Overview

NAP validates the whole resource graph at startup and aborts on the first problem, so a bad json
costs a full build/run cycle to discover. `nap_validate.py` catches the common failures **statically**
in a second. Run it on any data json you just touched, before launching.

## When to use

- Immediately after adding/editing objects in a data json (pairs with `nap-resource-graph`).
- Before running the app, as a pre-flight check.
- When triaging `Unknown object type <X>` or `Encountered embedded pointer that points to a
  non-embedded object` — the linter usually points straight at the offending object.

## Run it

```bash
python <path-to>/nap-skill-authoring/scripts/nap_validate.py <path-to>/data/objects.json
# from an app dir this is typically:
python .claude/skills/nap-skill-authoring/scripts/nap_validate.py data/objects.json
```

`--refresh` rebuilds the type/shape catalog (do this after adding a new module type or SDK update).
Exit code: 0 = no errors (warnings allowed), 1 = errors, 2 = could not run.

## Reading the output

- **ERROR** — NAP will reject this. Fix before running: malformed structure, an entry missing
  `"Type"`/`"mID"`, a duplicate mID, or an unknown `"Type"` (typo, missing `RequiredModule`, or a new
  `.cpp` that wasn't `regenerate.bat`'d).
- **WARN** — a heuristic derived from how the **demos** write the same type (the Embedded/Default flag
  and enum strings are not on disk for system modules, so this is inferred, not certain). A warning is
  usually a real bug — a property written as an inline object where every demo uses an id string (or
  vice versa), or an id reference pointing at no declared mID — but it can be a false positive on a
  genuine string literal the demos happen not to use. Read each one; don't blanket-ignore.

Fix findings with `nap-resource-graph` (copy the correct shape from a working block via
`nap_usage.py <Type>`). Diagnose runtime/build failures with `nap-build-run-verify`.

## What it cannot catch

It does not know a property's true Embedded/Default flag or an enum's valid strings (those live in
unshipped `.cpp` for system modules) — it approximates them from demo usage. It does not type-check
scalar values or validate that a graph is *useful* (only that it should *load*). A clean run means
"no detected structural/reference errors," not "the app does what you want."
