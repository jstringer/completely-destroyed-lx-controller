# NAP skill template

Copy this into a new `<skill-name>/SKILL.md` and fill it in. Delete guidance in _italics_.
Keep the body lean; move heavy API tables and long examples into `references/`.

```markdown
---
name: <skill-name>
description: Use when <triggering conditions — symptoms, tasks, and keywords a future agent would search: class names, "objects.json", RTTI error text, module names>. <One clause on the concept, no workflow summary.>
---

# <Title>

## Overview
_1–2 sentences: what this covers and the core principle. Name the concept, not the steps._

## When to use
_Bullet list of concrete triggers / symptoms. Include a "When NOT to use" line if there's an
adjacent skill that owns nearby cases._

## Mental model
_The minimum conceptual frame needed before touching anything. For NAP this is often the
resource↔instance split, the graph shape, or a lifecycle order._

## <The core content>
_Reference-style: tables for API/keys, one excellent worked example, exact JSON/C++ excerpts.
Every symbol here must be grounded (see below)._

## Finding the exact API
_Do not hardcode a full API dump, and do not guess a `Type` / property / enum string. Route to the
sources:_ copy a working block with `nap_usage.py <Type>` (the only complete source of the
Embedded/Default shape + enum strings for system-module types); get property names from headers
(walk the `RTTI_ENABLE` base chain) and docs URLs from `nap_doc.py`. Full mechanics:
`nap-skill-authoring/references/authoritative-sources.md`.

## Verify your change
_Include this whenever the skill produces JSON or C++._ State how the reader confirms the result
actually works — for NAP, run the app and read the `ResourceManager` load log; note rebuild gotchas
(`regenerate.bat` after a new `.cpp`, Napkin DLL lock) for app-authored types. See the project
`CLAUDE.md`.

## Common mistakes
_What goes wrong + the fix. Pull these from real NAP gotchas (RTTI errors, Embedded vs Default,
regenerate-after-new-.cpp, etc.), not hypotheticals._

## Worked examples
_Point at demos/app files by path (and `references/worked-examples.md` if long):
`<SDK>/demos/<demo>/...`. Mark any lxcontrol reference as an in-repo example, not the definition._
```

## Grounding checklist before you save
Run the verification checklist in `nap-skill-authoring/SKILL.md`. In short: every class, `"Type"`,
property, enum, signature, and URL is traceable to a source you actually read — headers/demos first,
then the resolver-produced docs URL. No guessed names, no hand-built URLs.
