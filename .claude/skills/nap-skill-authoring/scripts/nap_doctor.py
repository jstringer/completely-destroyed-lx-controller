#!/usr/bin/env python
"""Map a NAP build/load error to its cause and fix.

Paste a failing build or startup log; this matches known NAP failure signatures and prints the
cause + fix for each. It encodes the hard-won gotchas (see the project CLAUDE.md) as a deterministic
table, so you don't diagnose from memory.

    python nap_doctor.py --text "Unknown object type nap::FooComponent"
    python nap_doctor.py build.log            # read a file
    build.bat 2>&1 | python nap_doctor.py     # or pipe a log in
    python nap_doctor.py --list               # show every signature it knows

Stdlib only. Exit 0 = matched at least one signature, 1 = no known signature (prints guidance).
"""

import argparse
import re
import sys

# (compiled regex, title, cause, fix). Order matters: most specific first.
SIGNATURES = [
    (re.compile(r"LNK1104.*\.dll|cannot open file.*\.dll", re.I),
     "Linker can't open the module DLL (LNK1104)",
     "Napkin (the NAP editor) has the module DLL loaded for RTTI introspection and holds a lock on it.",
     "Close the project in Napkin, then re-run build.bat."),

    (re.compile(r"Unknown object type", re.I),
     "Unknown object type <X>",
     "The JSON \"Type\" isn't a registered class: a typo, not fully qualified (needs nap::...), its "
     "module isn't a RequiredModule, OR - for an app-authored type - a new .cpp was added but "
     "regenerate.bat wasn't run, so the class was never compiled in.",
     "Verify the exact registered name (nap_doc.py <Class> / nap_usage.py --module <Class>); confirm "
     "the module is in app.json/module.json RequiredModules; if you just added the .cpp, run "
     "regenerate.bat then build.bat. Lint the json with nap_validate.py."),

    (re.compile(r"[Ee]mbedded pointer that points to a non-embedded object"),
     "Encountered embedded pointer that points to a non-embedded object",
     "A pointer property's JSON shape doesn't match its RTTI flag: an inline object where Default "
     "(id string) is expected, or an id string where Embedded (inline object) is expected. The "
     "message does not name the property.",
     "Copy the correct shape from a working block: nap_usage.py <Type>. See "
     "nap-resource-graph/references/lookup-and-pointers.md. Lint with nap_validate.py to spot it."),

    (re.compile(r"unresolved external symbol|LNK2001|LNK2019", re.I),
     "Unresolved external symbol (link error)",
     "A referenced symbol wasn't linked: often a new .cpp not picked up (regenerate.bat not run), a "
     "class used across modules whose declaration lacks NAPAPI export, or a missing library.",
     "Run regenerate.bat after adding source files. If it's an SDK helper, check it's NAPAPI-exported "
     "(some, e.g. nap::MidiPortInfo, are not exported for out-of-module use)."),

    (re.compile(r"[Uu]nable to (open|load|find).*module|module .* not found", re.I),
     "Module failed to load",
     "A RequiredModule couldn't be found/loaded: missing from app.json/module.json, not built, or its "
     "own dependencies are missing.",
     "Add the module to RequiredModules, run regenerate.bat, rebuild. For a community module, confirm "
     "it's installed under modules/ (see modules.nap-framework.tech)."),

    (re.compile(r"[Uu]nable to (open|load|read).*(file|\.json|\.frag|\.vert|\.glsl)|No such file", re.I),
     "A referenced file/asset couldn't be opened",
     "A file path (shader, data json, texture, font) didn't resolve - usually relative to the app's "
     "data/ dir or a wrong working directory.",
     "Check the path is correct and the asset exists under data/. Run the app from its own directory."),

    (re.compile(r"duplicate|already exists.*mID|object with (the )?same (id|mID)", re.I),
     "Duplicate mID",
     "Two objects share an mID (must be unique across the loaded graph, including user_content.json).",
     "Rename one. nap_validate.py flags duplicates."),

    (re.compile(r"[Uu]nable to resolve.*pointer|[Cc]ould not resolve.*(id|link|object)", re.I),
     "Unresolved id reference (dangling pointer)",
     "A Default pointer names an mID that no loaded object declares (typo, or the target lives in a "
     "file that wasn't loaded).",
     "Fix the mID or declare the target. nap_validate.py flags probable dangling references."),
]


def diagnose(text):
    hits = []
    for rx, title, cause, fix in SIGNATURES:
        if rx.search(text):
            hits.append((title, cause, fix))
    return hits


def main(argv=None):
    ap = argparse.ArgumentParser(description="Diagnose a NAP build/load error.")
    ap.add_argument("file", nargs="?", help="log file to read (default: stdin)")
    ap.add_argument("--text", help="error text to diagnose directly")
    ap.add_argument("--list", action="store_true", help="list all known signatures")
    args = ap.parse_args(argv)

    if args.list:
        for _rx, title, cause, fix in SIGNATURES:
            print("- %s\n    cause: %s\n    fix:   %s\n" % (title, cause, fix))
        return 0

    if args.text is not None:
        text = args.text
    elif args.file:
        try:
            with open(args.file, "r", encoding="utf-8", errors="replace") as f:
                text = f.read()
        except OSError as e:
            print("error: %s" % e, file=sys.stderr)
            return 1
    else:
        text = sys.stdin.read()

    if not text.strip():
        print("no input. Pass --text \"...\", a log file, or pipe a log in. --list shows signatures.",
              file=sys.stderr)
        return 1

    hits = diagnose(text)
    if not hits:
        print("No known NAP signature matched. If this is a load-time failure, lint the data json "
              "with nap_validate.py; if it's a build failure, check regenerate.bat was run after any "
              "new .cpp and that Napkin is closed. `--list` shows all known signatures.")
        return 1

    for title, cause, fix in hits:
        print("### %s" % title)
        print("cause: %s" % cause)
        print("fix:   %s\n" % fix)
    return 0


if __name__ == "__main__":
    sys.exit(main())
