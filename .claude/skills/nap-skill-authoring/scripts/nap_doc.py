#!/usr/bin/env python
"""Resolve a NAP class / namespace / manual-page name to its exact docs URL.

The NAP reference (https://docs.nap-framework.tech) is Doxygen-generated: every
page lives under a NON-derivable hash prefix (e.g. nap::Material is at
/d2/de3/classnap_1_1_material). You cannot guess the URL from the name. This
script parses the Doxygen index pages once, caches the name->URL map next to
itself, and resolves names offline afterwards.

Stdlib only. Runs on the system `python` (the NAP SDK's bundled interpreter also
works). No third-party dependencies.

Usage:
    python nap_doc.py Material                # class (short name)
    python nap_doc.py nap::audio::AudioService # class (fully qualified, disambiguates)
    python nap_doc.py --ns nap::audio          # namespace
    python nap_doc.py --page rendering         # manual page (title substring)
    python nap_doc.py --list Material          # list class names containing substring
    python nap_doc.py --refresh Material       # re-pull the indexes, then resolve

Exit codes: 0 = at least one match printed, 1 = no match, 2 = network/parse error.
"""

import argparse
import html
import json
import os
import re
import sys
import urllib.request

MIN_CLASSES = 50  # sanity floor: fewer than this means the parse (or the page) is broken

BASE = "https://docs.nap-framework.tech"
CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".doc_cache", "index.json")
UA = {"User-Agent": "Mozilla/5.0 (nap_doc)"}

# One anchor shape covers every Doxygen index page: single-quoted, extensionless,
# absolute href, optional attributes (e.g. target='_self') before the closing '>'.
_ANCHOR = r"<a class='el' href='(/[^']+)'[^>]*>([^<]+)</a>"
# A class row pairs the class link with its namespace link: "Foo (nap::bar)".
_CLASS_ROW = _ANCHOR + r"\s*\(" + _ANCHOR + r"\)"


def _fetch(path):
    url = BASE + path
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=30) as r:
        return r.read().decode("utf-8", "replace")


def _ns_from_path(path):
    """/d8/df5/namespacenap_1_1audio -> nap::audio (namespaces are lowercase, safe)."""
    tail = path.rstrip("/").rsplit("/", 1)[-1]
    return tail.replace("namespace", "", 1).replace("_1_1", "::")


def _parse_classes(classes_html):
    """Every class/struct entry, namespaced or global, HTML-entities decoded."""
    out, seen = [], set()
    # Primary: "ClassName (namespace)" rows — the common, namespaced case.
    for cp, cn, _np, nn in re.findall(_CLASS_ROW, classes_html):
        out.append({"name": html.unescape(cn).strip(), "ns": html.unescape(nn).strip(), "path": cp})
        seen.add(cp)
    # Fallback: class/struct anchors with no trailing namespace link (global namespace,
    # or a layout the paired regex missed). ns = "" so lookups still work by short name.
    for path, text in re.findall(_ANCHOR, classes_html):
        if path in seen or ("/class" not in path and "/struct" not in path):
            continue
        out.append({"name": html.unescape(text).strip(), "ns": "", "path": path})
        seen.add(path)
    return out


def build_index():
    """Fetch and parse the three Doxygen index pages into a name->path map.

    Raises if the class index parses to almost nothing — that means the site layout
    changed or a non-HTML/challenge page came back with HTTP 200. We must NOT cache that:
    an empty cache turns every lookup into a false "no match" that reads as "doesn't exist".
    """
    classes = _parse_classes(_fetch("/classes.html"))
    if len(classes) < MIN_CLASSES:
        raise RuntimeError(
            "parsed only %d classes from /classes.html (expected >= %d). The Doxygen layout "
            "likely changed or a challenge/error page was returned; refusing to cache a broken "
            "index. Inspect the page and update the parser in %s."
            % (len(classes), MIN_CLASSES, os.path.basename(__file__)))

    seen = set()
    namespaces = []
    for path, _text in re.findall(_ANCHOR, _fetch("/namespaces.html")):
        if "namespace" not in path or path in seen:
            continue
        seen.add(path)
        namespaces.append({"fqn": _ns_from_path(path), "path": path})

    pages = [{"title": t, "path": p} for p, t in re.findall(_ANCHOR, _fetch("/pages.html"))]

    return {"base": BASE, "classes": classes, "namespaces": namespaces, "pages": pages}


def load_index(refresh=False):
    if not refresh and os.path.exists(CACHE):
        try:
            with open(CACHE, "r", encoding="utf-8") as f:
                idx = json.load(f)
            if len(idx.get("classes", [])) >= MIN_CLASSES:  # ignore a stale/broken cache
                return idx
        except (ValueError, OSError):
            pass  # corrupt cache -> rebuild
    idx = build_index()
    os.makedirs(os.path.dirname(CACHE), exist_ok=True)
    with open(CACHE, "w", encoding="utf-8") as f:
        json.dump(idx, f, indent=1)
    return idx


def selftest():
    """Resolve a few known classes and assert the URL shape. Exit 0 = pass."""
    idx = load_index()
    ok = True
    for name, tail in [("Material", "classnap_1_1_material"),
                       ("Scene", "classnap_1_1_scene"),
                       ("Entity", "classnap_1_1_entity")]:
        hits = resolve_class(idx, name)
        good = any(h["path"].endswith(tail) for h in hits)
        print("%-10s %s" % (name, "OK" if good else "FAIL"))
        ok = ok and good
    print("classes indexed: %d" % len(idx["classes"]))
    return 0 if ok else 1


def _url(path):
    return BASE + path


def resolve_class(idx, query):
    # Accept "Material" or "nap::audio::AudioService".
    if "::" in query:
        ns_q, name_q = query.rsplit("::", 1)
    else:
        ns_q, name_q = None, query
    name_l = name_q.lower()
    hits = [
        c for c in idx["classes"]
        if c["name"].lower() == name_l and (ns_q is None or c["ns"].lower() == ns_q.lower())
    ]
    return hits


def resolve_ns(idx, query):
    q = query.lower()
    exact = [n for n in idx["namespaces"] if n["fqn"].lower() == q]
    if exact:
        return exact
    # fall back to matching the last segment (e.g. "audio" -> nap::audio)
    return [n for n in idx["namespaces"] if n["fqn"].lower().rsplit("::", 1)[-1] == q]


def resolve_page(idx, query):
    q = query.lower()
    return [p for p in idx["pages"] if q in p["title"].lower()]


def main(argv=None):
    ap = argparse.ArgumentParser(description="Resolve a NAP name to its docs URL.")
    ap.add_argument("name", nargs="?", help="class name (short or fully-qualified)")
    ap.add_argument("--ns", metavar="NS", help="resolve a namespace, e.g. nap::audio")
    ap.add_argument("--page", metavar="Q", help="resolve a manual page by title substring")
    ap.add_argument("--list", metavar="SUB", dest="list_sub",
                    help="list class names containing SUB (disambiguation helper)")
    ap.add_argument("--refresh", action="store_true", help="re-pull the indexes first")
    ap.add_argument("--selftest", action="store_true", help="verify the index resolves known classes")
    args = ap.parse_args(argv)

    if args.selftest:
        try:
            return selftest()
        except Exception as e:
            print("selftest error: %s" % e, file=sys.stderr)
            return 2

    try:
        idx = load_index(refresh=args.refresh)
    except Exception as e:  # network down, parse failure, unwritable cache
        print("error: could not load doc index: %s" % e, file=sys.stderr)
        print("If offline, run once online to populate the cache: %s" % CACHE, file=sys.stderr)
        return 2

    if args.ns:
        hits = resolve_ns(idx, args.ns)
        for n in hits:
            print("%s -> %s" % (n["fqn"], _url(n["path"])))
        return 0 if hits else 1

    if args.page:
        hits = resolve_page(idx, args.page)
        for p in hits:
            print("%s -> %s" % (p["title"], _url(p["path"])))
        return 0 if hits else 1

    if args.list_sub:
        sub = args.list_sub.lower()
        names = sorted({"%s::%s" % (c["ns"], c["name"]) for c in idx["classes"]
                        if sub in c["name"].lower()})
        for n in names:
            print(n)
        return 0 if names else 1

    if not args.name:
        ap.print_help()
        return 1

    hits = resolve_class(idx, args.name)
    if hits:
        for c in hits:
            print("%s::%s -> %s" % (c["ns"], c["name"], _url(c["path"])))
        return 0

    # No exact match: offer substring suggestions.
    sub = args.name.rsplit("::", 1)[-1].lower()
    sugg = sorted({"%s::%s" % (c["ns"], c["name"]) for c in idx["classes"]
                   if sub in c["name"].lower()})[:15]
    print("no exact class match for %r" % args.name, file=sys.stderr)
    if sugg:
        print("did you mean:", file=sys.stderr)
        for s in sugg:
            print("  " + s, file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
