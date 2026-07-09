---
name: nap-skill-authoring
description: Use when writing, editing, or reviewing a Claude Code skill about the NAP Framework — anything documenting NAP classes, resources/entities/components, objects.json, RTTI, Napkin, the nap:: API, or the napartnet/napmidi/napsequence/napaudio/naprender modules. The method that keeps NAP skills from hallucinating API.
---

# Authoring NAP Framework Skills

## Overview

NAP has a large C++ API and a declarative JSON model. Its online reference is
Doxygen-generated with **non-derivable hashed URLs** — `nap::Material` lives at
`/d2/de3/classnap_1_1_material`, and the `/d2/de3/` hash cannot be guessed from the
name. A skill that guesses URLs or invents class/method/property names produces
confident, wrong guidance.

**The Iron Rule of NAP skills: every NAP API claim must be traceable to an authoritative
source at authoring time.** Class names, property names, JSON `"Type"` strings, method
signatures, enum values, module names — if you did not read it in a source, do not write
it down. Grounding is not optional and "it's probably called that" is not grounding.

See `references/authoritative-sources.md` for the sources and how to look each one up
(including the `scripts/nap_doc.py` URL resolver and its manual fallback).

## Authoring workflow

1. **Scope it.** One skill = one recurring NAP task or concept (adding a component,
   composing the resource graph, wiring a sequence). If you can't name the trigger in one
   sentence, it's two skills.
2. **Gather ground truth** for every symbol the skill will name, using the source ladder in
   `references/authoritative-sources.md`. Know the key constraint: **system modules ship as
   headers only** (no `.cpp`), so the Embedded/Default pointer shape and serialized enum strings
   for the common types are **not on disk and not on the docs page** — they come from a working
   JSON block (`scripts/nap_usage.py`). So the front-line technique is *copy a working demo/app
   block and adapt it*, backed by headers (property names — walk the `RTTI_ENABLE` base chain),
   `nap_doc.py` (docs URLs), and `.cpp` grep only for app/demo-authored types. Collect `file:line`
   citations and real JSON excerpts as you go.
3. **Draft to the template.** Copy `references/skill-template.md` and fill it in. Keep
   SKILL.md lean; push long API tables and worked examples into `references/`.
4. **Self-audit against sources** (see checklist). Do not skip because it "looks right".
5. **Verify with a fresh agent** — a retrieval/application scenario (below).

## Conventions

- **Location & naming.** `apps/lxcontrol/.claude/skills/<name>/SKILL.md`; `<name>` is
  lowercase-hyphen, verb-first or concept-first (`nap-resource-graph`, `nap-adding-a-component`),
  and matches the directory. `name:` in frontmatter matches it exactly.
- **NAP-general, lives here.** Write portable NAP knowledge. lxcontrol may be cited only as a
  clearly-marked in-repo worked example ("lxcontrol example:"), never as the definition of how
  NAP works. Someone should be able to lift the skill into another NAP app unchanged.
- **description = triggers only.** Third person, starts with "Use when…", lists symptoms and
  keywords a future agent would search (class names, `objects.json`, RTTI errors). Never
  summarize the workflow — agents follow a summarized description instead of reading the skill.
- **Altitude.** SKILL.md is a map, not a manual. Reference files hold the heavy detail; the
  script holds anything mechanical. One excellent example beats five.
- **Share, don't duplicate.** Point at `nap-skill-authoring/references/authoritative-sources.md`
  for lookup mechanics rather than re-explaining them in each skill.

## Verifying a NAP skill

Structural:
- [ ] Frontmatter valid; `name` matches the directory; `description` is third-person, trigger-only.

Accuracy (the part that matters):
- [ ] Every NAP class / `"Type"` named resolves via `scripts/nap_doc.py` **or** exists in local
      headers/demos (grep it — do not trust memory).
- [ ] Every property **name** came from a header (including inherited ones — walk the
      `RTTI_ENABLE` base chain) or the docs member table — not inferred from a demo alone.
- [ ] Every **Embedded/Default shape** and **enum string** came from a working JSON block
      (`nap_usage.py`) or an app-type's `.cpp` — never from a header, the docs page, or a guess
      (those don't carry it for system modules).
- [ ] Every cited docs/module URL was produced by the resolver or actually fetched (200), never
      hand-assembled from a hash.
- [ ] Every `file:line` demo citation points at real, current code in THIS SDK.
- [ ] If the skill changes JSON or C++, it tells the reader how to **verify the change loads/runs**
      (run the app, read the load log; rebuild gotchas for app types).

Behavioral (fresh-context test — the RED/GREEN):
- [ ] Give a fresh agent a realistic task the skill targets, **without** the skill: does it guess
      a `Type` or property? (baseline failure)
- [ ] Same task **with** the skill: does it now look the symbol up and cite a source? (pass)

## Red flags — STOP, you are about to hallucinate

- Writing a class/property/`"Type"` name you did not just read in a source.
- Hand-assembling a docs URL, or pasting one a summarizer (e.g. WebFetch) "quoted" — those hashes
  are fabricated unless the resolver or a real fetch produced them.
- "The API is probably…", "by convention NAP would…", "this is basically like <other framework>".
- Citing online API without checking it against the local 0.8.0 headers (online may be a newer/older
  version than the SDK on disk).

All of these mean: stop and resolve the symbol against a source before writing it.
