#!/usr/bin/env python
"""Scaffold a NAP module class (Component or Resource) with correct RTTI boilerplate.

The RTTI macros and the resource<->instance split are exactly what's easy to get subtly wrong by
hand. This emits the canonical structure (verified against this SDK's demos and the lxcontrol
module) so you only fill in logic. It also resolves each pointer target's header via the SDK source,
so the #includes are right.

    python nap_new_component.py --name SpinComponent --module lxcontrol \
        --desc "spins a transform" \
        --prop Speed:float --prop Enabled:bool \
        --ptr Curve:nap::math::FloatFCurve \
        --embed Settings:nap::SomeSettings

    python nap_new_component.py --name PaletteResource --kind resource --prop Size:int
    python nap_new_component.py ... --dry-run     # print to stdout instead of writing files

Writes <Name>.h and <Name>.cpp into the app module's src/ (or --out DIR). Prints the mandatory
`regenerate.bat` reminder — a new .cpp not regenerated is the classic "Unknown object type" cause.

Stdlib only. Exit 0 = generated, 2 = bad args / SDK not found.
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nap_usage  # noqa: E402  (SDK-root finder + declaration finder)

_SCALAR_DEFAULT = {"float": " = 0.0f", "double": " = 0.0", "int": " = 0",
                   "int32": " = 0", "uint": " = 0", "bool": " = true"}


def _member_type(cpp_type, is_ptr):
    return "ResourcePtr<%s>" % _short(cpp_type) if is_ptr else cpp_type


def _short(t):
    return t.rsplit("::", 1)[-1]


def _member_line(p):
    # <type> m<Name>[ = default];\t\t///< Property: 'Name'   (default already includes " = ")
    return "\t\t%s m%s%s;\t\t///< Property: '%s'" % (p["mtype"], p["name"], p["default"], p["name"])


def _header_for(sdk, target):
    """(#include line, is_local) for a pointer target, resolved from SDK source."""
    decls = nap_usage.find_declaration(sdk, target)
    if not decls:
        return ("// TODO: #include the header declaring %s" % target, False)
    rel, _line, module = decls[0]
    base = os.path.basename(rel)
    if module.startswith("app-authored"):
        return ('#include "%s"' % base, True)
    return ("#include <%s>" % base, False)


def _props(prop_args, ptr_args, embed_args):
    """-> list of dicts: {name, member_type, default, flag, target?}"""
    out = []
    for spec in prop_args or []:
        name, _, ctype = spec.partition(":")
        if not ctype:
            raise ValueError("--prop must be Name:cpptype (got %r)" % spec)
        out.append({"name": name, "mtype": ctype, "default": _SCALAR_DEFAULT.get(ctype, ""),
                    "flag": "nap::rtti::EPropertyMetaData::Default", "target": None})
    for spec in ptr_args or []:
        name, _, target = spec.partition(":")
        if not target:
            raise ValueError("--ptr must be Name:nap::Target (got %r)" % spec)
        out.append({"name": name, "mtype": "ResourcePtr<%s>" % _short(target), "default": "",
                    "flag": "nap::rtti::EPropertyMetaData::Default", "target": target})
    for spec in embed_args or []:
        name, _, target = spec.partition(":")
        if not target:
            raise ValueError("--embed must be Name:nap::Target (got %r)" % spec)
        out.append({"name": name, "mtype": "ResourcePtr<%s>" % _short(target), "default": "",
                    "flag": "nap::rtti::EPropertyMetaData::Required | nap::rtti::EPropertyMetaData::Embedded",
                    "target": target})
    return out


def gen_component(name, desc, props, includes):
    inst = name + "Instance"
    members = "\n".join(_member_line(p) for p in props)
    inc_block = ("\n".join(sorted(set(includes))) + "\n") if includes else ""
    header = '''#pragma once

// External Includes
#include <component.h>
%s
namespace nap
{
\tclass %sInstance;

\t/**
\t * %s
\t */
\tclass NAPAPI %s : public Component
\t{
\t\tRTTI_ENABLE(Component)
\t\tDECLARE_COMPONENT(%s, %sInstance)
\tpublic:
\t\t%s() : Component() { }

%s
\t};


\tclass NAPAPI %s : public ComponentInstance
\t{
\t\tRTTI_ENABLE(ComponentInstance)
\tpublic:
\t\t%s(EntityInstance& entity, Component& resource) :
\t\t\tComponentInstance(entity, resource) { }

\t\tvirtual bool init(utility::ErrorState& errorState) override;
\t\tvirtual void update(double deltaTime) override;
\t};
}
''' % (inc_block, name, desc, name, name, name, name, members, inst, inst)

    rtti_props = "\n".join('\tRTTI_PROPERTY("%s", &nap::%s::m%s, %s)'
                           % (p["name"], name, p["name"], p["flag"]) for p in props)
    cpp = '''#include "%s.h"

#include <entity.h>

RTTI_BEGIN_CLASS(nap::%s, "%s")
%s
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::%s)
\tRTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)
RTTI_END_CLASS

namespace nap
{
\tbool %s::init(utility::ErrorState& errorState)
\t{
\t\t// TODO: fetch resource properties via getComponent<%s>() and cache sibling
\t\t// instances via getEntityInstance()->findComponent<T>()
\t\treturn true;
\t}


\tvoid %s::update(double deltaTime)
\t{
\t}
}
''' % (name, name, desc, rtti_props, inst, inst, name, inst)
    return header, cpp


def gen_resource(name, desc, props, includes):
    members = "\n".join(_member_line(p) for p in props)
    inc_block = ("\n".join(sorted(set(includes))) + "\n") if includes else ""
    header = '''#pragma once

// External Includes
#include <nap/resource.h>
%s
namespace nap
{
\t/**
\t * %s
\t */
\tclass NAPAPI %s : public Resource
\t{
\t\tRTTI_ENABLE(Resource)
\tpublic:
\t\tvirtual bool init(utility::ErrorState& errorState) override;

%s
\t};
}
''' % (inc_block, desc, name, members)

    rtti_props = "\n".join('\tRTTI_PROPERTY("%s", &nap::%s::m%s, %s)'
                           % (p["name"], name, p["name"], p["flag"]) for p in props)
    cpp = '''#include "%s.h"

RTTI_BEGIN_CLASS(nap::%s, "%s")
%s
RTTI_END_CLASS

namespace nap
{
\tbool %s::init(utility::ErrorState& errorState)
\t{
\t\treturn true;
\t}
}
''' % (name, name, desc, rtti_props, name)
    return header, cpp


def main(argv=None):
    ap = argparse.ArgumentParser(description="Scaffold a NAP Component/Resource class with RTTI.")
    ap.add_argument("--name", required=True, help="class name, e.g. SpinComponent")
    ap.add_argument("--kind", choices=["component", "resource"], default="component")
    ap.add_argument("--desc", default="TODO: describe this class")
    ap.add_argument("--module", help="app name whose module/src to write into (e.g. lxcontrol)")
    ap.add_argument("--out", help="explicit output directory (overrides --module)")
    ap.add_argument("--prop", action="append", help="scalar property Name:cpptype (repeatable)")
    ap.add_argument("--ptr", action="append", help="Default pointer property Name:nap::Target (repeatable)")
    ap.add_argument("--embed", action="append", help="Embedded pointer property Name:nap::Target (repeatable)")
    ap.add_argument("--dry-run", action="store_true", help="print to stdout instead of writing files")
    ap.add_argument("--force", action="store_true", help="overwrite existing files")
    args = ap.parse_args(argv)

    sdk = nap_usage.find_sdk_root()
    if not sdk:
        print("error: could not locate the SDK root", file=sys.stderr)
        return 2

    try:
        props = _props(args.prop, args.ptr, args.embed)
    except ValueError as e:
        print("error: %s" % e, file=sys.stderr)
        return 2

    includes = [_header_for(sdk, p["target"])[0] for p in props if p["target"]]
    if args.kind == "component":
        header, cpp = gen_component(args.name, args.desc, props, includes)
    else:
        header, cpp = gen_resource(args.name, args.desc, props, includes)

    if args.dry_run:
        print("=== %s.h ===\n%s\n=== %s.cpp ===\n%s" % (args.name, header, args.name, cpp))
        return 0

    out_dir = args.out
    if not out_dir and args.module:
        cand = os.path.join(sdk, "apps", args.module, "module", "src")
        out_dir = cand if os.path.isdir(cand) else None
    if not out_dir:
        print("error: no output dir - pass --out DIR or --module <app-with-module/src>", file=sys.stderr)
        return 2

    written = []
    for ext, content in ((".h", header), (".cpp", cpp)):
        path = os.path.join(out_dir, args.name + ext)
        if os.path.exists(path) and not args.force:
            print("error: %s exists (use --force)" % path, file=sys.stderr)
            return 2
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(content)
        written.append(path)

    for p in written:
        print("wrote %s" % p)
    print("\nNEXT: run regenerate.bat (a new .cpp not regenerated compiles to nothing and the app "
          "fails to load with \"Unknown object type nap::%s\"), then build.bat. Close Napkin first "
          "(it locks the module DLL)." % args.name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
