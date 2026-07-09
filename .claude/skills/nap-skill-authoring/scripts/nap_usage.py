#!/usr/bin/env python
"""Find working JSON usage of a NAP type, and the module/header that declares it.

The single most reliable way to write correct NAP JSON is to copy a block from a demo (or
another app) whose data json ALREADY uses the type and adapt it: that block provably loads,
so it carries the exact property names, the Embedded (inline object) vs Default (id string)
shape, and the exact serialized enum strings — none of which are obtainable from system-module
sources, which ship as headers only (no .cpp / no RTTI_PROPERTY / no RTTI_ENUM_VALUE on disk).

This script does that lookup:
    python nap_usage.py nap::RotateComponent   # print every demo/app JSON object of this Type
    python nap_usage.py RotateComponent        # short name also works (matches the Type suffix)
    python nap_usage.py --module RotateComponent  # which header/module declares it
    python nap_usage.py --all nap::Material     # show every match, not just the first few

Stdlib only. Locates the SDK root by walking up until it finds demos/ + system_modules/.
Exit 0 = found something, 1 = nothing found, 2 = could not locate the SDK.
"""

import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MAX_BLOCKS_DEFAULT = 5
MAX_BLOCK_LINES = 120


def find_sdk_root(start=HERE):
    """Walk up until a directory contains both demos/ and system_modules/ (the SDK root)."""
    d = start
    while True:
        if os.path.isdir(os.path.join(d, "demos")) and os.path.isdir(os.path.join(d, "system_modules")):
            return d
        parent = os.path.dirname(d)
        if parent == d:
            return None
        d = parent


def _short(type_str):
    return type_str.rsplit("::", 1)[-1]


def _json_files(sdk):
    """All app-authored data json in the SDK: demos/*/data and apps/*/data (recursive)."""
    roots = [os.path.join(sdk, "demos"), os.path.join(sdk, "apps")]
    for root in roots:
        for dirpath, _dirs, files in os.walk(root):
            # only look under a data/ directory; skip build output
            parts = dirpath.replace("\\", "/").split("/")
            if "data" not in parts or "bin" in parts or "bin_package" in parts or "msvc64" in parts:
                continue
            for fn in files:
                if fn.endswith(".json"):
                    yield os.path.join(dirpath, fn)


def _walk_objects(node):
    """Yield every dict that has a "Type" key, anywhere in the JSON tree."""
    if isinstance(node, dict):
        if "Type" in node and isinstance(node["Type"], str):
            yield node
        for v in node.values():
            yield from _walk_objects(v)
    elif isinstance(node, list):
        for v in node:
            yield from _walk_objects(v)


def find_usages(sdk, type_query):
    """Return [(relpath, object_dict)] for every JSON object whose Type matches."""
    short = _short(type_query).lower()
    fq = type_query.lower() if "::" in type_query else None
    hits = []
    for path in _json_files(sdk):
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
        except (ValueError, OSError):
            continue
        for obj in _walk_objects(data):
            t = obj["Type"].lower()
            if (fq and t == fq) or (not fq and _short(t) == short):
                hits.append((os.path.relpath(path, sdk), obj))
    return hits


def find_declaration(sdk, type_query):
    """Grep headers/module source for the class declaration; report header + module."""
    name = _short(type_query)
    pat = re.compile(r"\bclass\s+(?:NAPAPI\s+)?%s\b" % re.escape(name))
    search_roots = [os.path.join(sdk, "include"),
                    os.path.join(sdk, "system_modules"),
                    os.path.join(sdk, "modules"),
                    os.path.join(sdk, "apps")]
    results = []
    for root in search_roots:
        for dirpath, _dirs, files in os.walk(root):
            for fn in files:
                if not (fn.endswith(".h") or fn.endswith(".hpp")):
                    continue
                fpath = os.path.join(dirpath, fn)
                try:
                    with open(fpath, "r", encoding="utf-8", errors="replace") as f:
                        for i, line in enumerate(f, 1):
                            if not pat.search(line):
                                continue
                            stripped = line.strip()
                            # skip a bare forward declaration ("class Foo;") — not the definition
                            if stripped.endswith(";") and ":" not in stripped and "{" not in stripped:
                                continue
                            rel = os.path.relpath(fpath, sdk).replace("\\", "/")
                            results.append((rel, i, _module_of(rel)))
                            break
                except OSError:
                    continue
    return results


def _module_of(relpath):
    """Infer the owning module from a header's path."""
    parts = relpath.split("/")
    for anchor in ("system_modules", "modules"):
        if anchor in parts:
            i = parts.index(anchor)
            if i + 1 < len(parts):
                return parts[i + 1]
    if parts and parts[0] == "include":
        return "napcore/naprtti (SDK core headers)"
    if "apps" in parts and "module" in parts:
        return "app-authored module (%s)" % parts[parts.index("apps") + 1]
    return "unknown"


def main(argv=None):
    ap = argparse.ArgumentParser(description="Find working JSON usage / declaring module of a NAP type.")
    ap.add_argument("type", help="NAP type, short or fully-qualified (e.g. nap::RotateComponent)")
    ap.add_argument("--module", action="store_true", help="report the declaring header/module instead of JSON usage")
    ap.add_argument("--all", action="store_true", help="show every JSON match (default: first %d)" % MAX_BLOCKS_DEFAULT)
    args = ap.parse_args(argv)

    sdk = find_sdk_root()
    if not sdk:
        print("error: could not locate the SDK root (no demos/ + system_modules/ above %s)" % HERE,
              file=sys.stderr)
        return 2

    if args.module:
        decls = find_declaration(sdk, args.type)
        if not decls:
            print("no header declaration found for %r (may be a template alias or a runtime-only "
                  "type; grep the source directly)" % args.type, file=sys.stderr)
            return 1
        for rel, line, module in decls:
            print("%s:%d    module: %s" % (rel, line, module))
        return 0

    hits = find_usages(sdk, args.type)
    if not hits:
        print("no JSON usage of %r found in demos/ or apps/. It may be runtime-constructed, "
              "app-authored, or unused in examples - check the class reference / headers." % args.type,
              file=sys.stderr)
        return 1

    shown = hits if args.all else hits[:MAX_BLOCKS_DEFAULT]
    print("# %d JSON usage(s) of %s (showing %d):\n" % (len(hits), args.type, len(shown)))
    for rel, obj in shown:
        block = json.dumps(obj, indent=4)
        lines = block.splitlines()
        if len(lines) > MAX_BLOCK_LINES:
            block = "\n".join(lines[:MAX_BLOCK_LINES]) + "\n    ... (%d more lines; open the file)" % (
                len(lines) - MAX_BLOCK_LINES)
        print("## %s" % rel)
        print(block)
        print()
    if not args.all and len(hits) > len(shown):
        print("# %d more not shown; pass --all to see them." % (len(hits) - len(shown)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
