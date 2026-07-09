#!/usr/bin/env python
"""Pre-flight linter for a NAP data json — catch load errors before build/run.

NAP validates the whole resource graph at startup and aborts on the first problem. This
script catches the common ones statically:

  ERRORS (high confidence — NAP will reject these):
    - malformed top level (no "Objects" array), or an entry missing "Type" / "mID"
    - duplicate mID
    - unknown "Type" (not a registered SDK class, not an app/demo-authored class, and never
      seen in any demo/app json) -> the usual cause of "Unknown object type <X>"

  WARNINGS (heuristic — derived from how the DEMOS write the same type, since the
  Embedded/Default flag is not on disk for system modules):
    - a property written with a different shape (inline object vs id string) than every demo
      uses -> the usual cause of "Encountered embedded pointer that points to a non-embedded object"
    - a probable id-reference (a property demos write as an mID) whose value resolves to no
      declared mID -> a dangling reference
    - an entity declared but never spawned by a Scene and never used as a child

Stdlib only. Reuses the docs index (nap_doc) and the demo/app scan (nap_usage).

    python nap_validate.py ../../../../data/objects.json
    python nap_validate.py --refresh <file>   # rebuild the type/shape catalog first

Exit 0 = no errors (warnings allowed), 1 = errors found, 2 = could not run.
"""

import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nap_doc      # noqa: E402  (URL/index cache + class list)
import nap_usage    # noqa: E402  (SDK-root finder + json scan)

CATALOG = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".doc_cache", "validate_catalog.json")
_RTTI_DECL = re.compile(r"RTTI_BEGIN_(?:CLASS|STRUCT)(?:_NO_DEFAULT_CONSTRUCTOR)?\s*\(\s*([\w:]+)"
                        r"|DEFINE_GROUP\s*\(\s*([\w:]+)")
REF_RATIO = 0.6         # a (Type,prop) whose string values are mostly mIDs is a reference property
IGNORE_KEYS = {"Type", "mID", "Name"}  # Name is a free label, not a graph reference


# --------------------------------------------------------------------------- catalog build

def _registered_from_source(sdk):
    """Grep app/demo/local module source for RTTI_BEGIN_CLASS / STRUCT / DEFINE_GROUP names."""
    names = set()
    for root in ("apps", "demos", "modules", "include"):
        base = os.path.join(sdk, root)
        for dirpath, _dirs, files in os.walk(base):
            if any(p in dirpath.replace("\\", "/").split("/") for p in ("msvc64", "bin", "bin_package")):
                continue
            for fn in files:
                if not (fn.endswith(".cpp") or fn.endswith(".h")):
                    continue
                try:
                    with open(os.path.join(dirpath, fn), "r", encoding="utf-8", errors="replace") as f:
                        for m in _RTTI_DECL.finditer(f.read()):
                            names.add(m.group(1) or m.group(2))
                except OSError:
                    continue
    return names


def build_catalog(sdk):
    """Registered type names + per-(Type,property) value-shape stats, from docs + demos/apps."""
    idx = nap_doc.load_index()
    types = set()
    for c in idx["classes"]:
        types.add(("%s::%s" % (c["ns"], c["name"])) if c["ns"] else c["name"])
    types |= _registered_from_source(sdk)

    shapes = {}  # "Type\x00prop" -> {"string":n,"object":n,"array":n,"scalar":n,"strings":n,"refhits":n}
    seen_types = set()
    for path in nap_usage._json_files(sdk):
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
        except (ValueError, OSError):
            continue
        file_ids = {o["mID"] for o in nap_usage._walk_objects(data) if isinstance(o.get("mID"), str)}
        for obj in nap_usage._walk_objects(data):
            t = obj["Type"]
            seen_types.add(t)
            for k, v in obj.items():
                if k in IGNORE_KEYS:
                    continue
                rec = shapes.setdefault(t + "\x00" + k,
                                        {"string": 0, "object": 0, "array": 0, "scalar": 0,
                                         "strings": 0, "refhits": 0})
                if isinstance(v, str):
                    rec["string"] += 1
                    rec["strings"] += 1
                    if v in file_ids:
                        rec["refhits"] += 1
                elif isinstance(v, dict):
                    rec["object"] += 1
                elif isinstance(v, list):
                    rec["array"] += 1
                else:
                    rec["scalar"] += 1
    types |= seen_types  # template aliases (ResourceGroup) etc. that only appear in json
    return {"types": sorted(types), "shapes": shapes}


def load_catalog(sdk, refresh=False):
    if not refresh and os.path.exists(CATALOG):
        try:
            with open(CATALOG, "r", encoding="utf-8") as f:
                cat = json.load(f)
            if cat.get("types"):
                cat["types"] = set(cat["types"])
                return cat
        except (ValueError, OSError):
            pass
    cat = build_catalog(sdk)
    os.makedirs(os.path.dirname(CATALOG), exist_ok=True)
    with open(CATALOG, "w", encoding="utf-8") as f:
        json.dump({"types": sorted(cat["types"]), "shapes": cat["shapes"]}, f)
    return cat


# --------------------------------------------------------------------------- validation

def _shape_of(v):
    if isinstance(v, str):
        return "string"
    if isinstance(v, dict):
        return "object"
    if isinstance(v, list):
        return "array"
    return "scalar"


def validate(target_path, cat):
    errors, warnings = [], []
    try:
        with open(target_path, "r", encoding="utf-8") as f:
            data = json.load(f)
    except ValueError as e:
        return (["invalid JSON: %s" % e], [])
    except OSError as e:
        return (["cannot read %s: %s" % (target_path, e)], [])

    if not isinstance(data, dict) or not isinstance(data.get("Objects"), list):
        errors.append('top level must be an object with an "Objects" array')
        return errors, warnings

    # collect declared mIDs + duplicate detection
    all_ids, dup = set(), set()
    for obj in nap_usage._walk_objects(data):
        mid = obj.get("mID")
        if isinstance(mid, str):
            if mid in all_ids:
                dup.add(mid)
            all_ids.add(mid)
    for d in sorted(dup):
        errors.append('duplicate mID "%s"' % d)

    # top-level entries must have Type + mID
    for i, entry in enumerate(data["Objects"]):
        if not isinstance(entry, dict) or "Type" not in entry:
            errors.append('Objects[%d] has no "Type"' % i)
        elif "mID" not in entry:
            errors.append('Objects[%d] (%s) has no "mID"' % (i, entry["Type"]))

    # unknown types + per-property shape/reference checks
    for obj in nap_usage._walk_objects(data):
        t = obj["Type"]
        if t not in cat["types"]:
            errors.append('unknown Type "%s" (typo, missing RequiredModule, or new .cpp not '
                          'regenerated) in mID "%s"' % (t, obj.get("mID", "?")))
            continue
        for k, v in obj.items():
            if k in IGNORE_KEYS:
                continue
            rec = cat["shapes"].get(t + "\x00" + k)
            if not rec:
                continue  # property never seen in demos — can't judge
            observed = {s for s in ("string", "object", "array", "scalar") if rec[s] > 0}
            got = _shape_of(v)
            if observed and got not in observed:
                warnings.append('%s.%s is written as %s here, but demos always use %s '
                                '(Embedded/Default or value-type mismatch?)'
                                % (t, k, got, "/".join(sorted(observed))))
            elif got == "string" and rec["strings"] and rec["refhits"] / rec["strings"] >= REF_RATIO:
                if v and v not in all_ids:
                    warnings.append('%s.%s = "%s" looks like an id reference but no object declares '
                                    'that mID (dangling reference?)' % (t, k, v))

    # entities declared but never spawned/childed
    entity_ids = {o["mID"] for o in nap_usage._walk_objects(data)
                  if o.get("Type") == "nap::Entity" and isinstance(o.get("mID"), str)}
    spawned = set()
    for obj in nap_usage._walk_objects(data):
        if obj.get("Type") == "nap::Scene":
            for e in obj.get("Entities", []):
                if isinstance(e, dict) and isinstance(e.get("Entity"), str):
                    spawned.add(e["Entity"])
        for child in obj.get("Children", []) or []:
            if isinstance(child, str):
                spawned.add(child)
    for e in sorted(entity_ids - spawned):
        warnings.append('entity "%s" is never spawned by a Scene or used as a child '
                        '(it will not instantiate)' % e)

    return errors, warnings


def main(argv=None):
    ap = argparse.ArgumentParser(description="Lint a NAP data json before running.")
    ap.add_argument("file", help="path to the data json (e.g. data/objects.json)")
    ap.add_argument("--refresh", action="store_true", help="rebuild the type/shape catalog first")
    args = ap.parse_args(argv)

    sdk = nap_usage.find_sdk_root()
    if not sdk:
        print("error: could not locate the SDK root", file=sys.stderr)
        return 2
    try:
        cat = load_catalog(sdk, refresh=args.refresh)
    except Exception as e:
        print("error building catalog: %s" % e, file=sys.stderr)
        return 2

    errors, warnings = validate(args.file, cat)
    for w in warnings:
        print("WARN:  " + w)
    for e in errors:
        print("ERROR: " + e)
    print("\n%d error(s), %d warning(s)  [%s]"
          % (len(errors), len(warnings), os.path.basename(args.file)))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
