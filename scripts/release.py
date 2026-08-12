#!/usr/bin/env python
"""
Build lxcontrol and publish the packaged zip to a GitHub release.

    python scripts/release.py               # draft release, tag from app.json Version
    python scripts/release.py --publish     # make it live
    python scripts/release.py --version 0.2.0 --notes "First unified-effects build"

This is also what CI runs: .github/workflows/release.yml fetches the prebuilt NAP SDK, moves the
checkout to <sdk>/apps/lxcontrol (apps must live there -- see the NAP project-management docs), and
calls this script. One code path for local and hosted releases.

The prebuilt SDK is self-contained, Vulkan included, at
system_modules/naprender/thirdparty/vulkansdk -- nothing else has to be installed to build.

ponytail: no build-system abstraction, no config file. One subprocess call and `gh`.
"""

import argparse
import glob
import json
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

APP_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SDK_DIR = os.path.dirname(os.path.dirname(APP_DIR))
PYTHON = os.path.join(SDK_DIR, "thirdparty", "python", "msvc", "x86_64", "python.exe")
BUILDSYS = os.path.join(SDK_DIR, "tools", "buildsystem", "common")

# Build-machine detritus that must never ship in a release: whatever patches happened to be authored
# on this machine, and the dead preset files from the superseded Preset era. Packaging installs
# everything under data/, so these get moved aside for the duration of the build.
SCRUB = ["data/user_content.json", "data/user_content.session", "data/presets"]


def run(cmd, **kw):
    print("+ " + " ".join(str(c) for c in cmd))
    return subprocess.run(cmd, check=True, **kw)


def sdk_python(script, *args):
    """The buildsystem must run under the SDK's bundled interpreter, with the host's PYTHONPATH/HOME
    cleared -- inheriting them makes the bundled interpreter import the wrong stdlib."""
    env = dict(os.environ)
    env.pop("PYTHONPATH", None)
    env.pop("PYTHONHOME", None)
    run([PYTHON, os.path.join(BUILDSYS, script), APP_DIR, *args], env=env, cwd=APP_DIR)


def git(*args):
    return subprocess.run(["git", *args], cwd=APP_DIR, capture_output=True, text=True).stdout.strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", help="override app.json Version")
    ap.add_argument("--notes", default="", help="release notes body")
    ap.add_argument("--publish", action="store_true", help="publish instead of leaving a draft")
    ap.add_argument("--allow-dirty", action="store_true", help="package an uncommitted tree anyway")
    args = ap.parse_args()

    if not os.path.isfile(PYTHON):
        sys.exit("No NAP SDK found at %s -- this repo must sit at <nap-sdk>/apps/lxcontrol." % SDK_DIR)

    # A release has to be reproducible from a commit, so refuse to build a dirty tree by default.
    dirty = git("status", "--porcelain")
    if dirty and not args.allow_dirty:
        sys.exit("Working tree is dirty; commit first or pass --allow-dirty:\n" + dirty)

    with open(os.path.join(APP_DIR, "app.json")) as f:
        version = args.version or json.load(f)["Version"]
    tag = "v" + version
    sha = git("rev-parse", "--short", "HEAD")
    print("Releasing %s (%s)" % (tag, sha))

    # Drop stale zips so "newest zip" below can't pick up a previous run's artifact.
    for old in glob.glob(os.path.join(APP_DIR, "lxcontrol-*.zip")):
        os.remove(old)

    # Park scrubbed content OUTSIDE the app tree. Renaming it in place does not work: packaging installs
    # everything under data/ wholesale, so a data/presets.release-bak just ships under a worse name.
    stash = tempfile.mkdtemp(prefix="lxcontrol-release-")
    moved = []
    try:
        for rel in SCRUB:
            path = os.path.join(APP_DIR, rel)
            if os.path.exists(path):
                shutil.move(path, os.path.join(stash, os.path.basename(rel)))
                moved.append((os.path.join(stash, os.path.basename(rel)), path))
                print("scrubbed %s (build-machine content, not shipped)" % rel)

        # Packaging is self-sufficient: package_app_by_name.py runs its own cmake configure and
        # `cmake --build . --target install --config Release`. Calling regenerate + build first just
        # built everything twice -- it configures fresh, so new source files are picked up regardless.
        sdk_python("package_app_by_dir.py", "-ns", "-nn", "-np")
    finally:
        for src, dst in moved:
            shutil.move(src, dst)
        shutil.rmtree(stash, ignore_errors=True)

    produced = glob.glob(os.path.join(APP_DIR, "lxcontrol-*.zip"))
    if not produced:
        sys.exit("Packaging reported success but produced no zip.")

    # Verify the scrub actually took. The first version of it renamed files in place inside data/, which
    # packaging happily shipped anyway -- a silent failure that put local test patches in a release.
    entries = zipfile.ZipFile(produced[0]).namelist()
    leaked = [n for n in entries if "user_content" in n or "/presets/" in n or "presets.release" in n]
    if leaked:
        sys.exit("Refusing to release: build-machine content leaked into the package:\n  " +
                 "\n  ".join(leaked[:10]))
    if not any(n.endswith("/lxcontrol.exe") for n in entries):
        sys.exit("Refusing to release: no lxcontrol.exe in the package.")
    print("package check: %d entries, exe present, no local content" % len(entries))

    # The default name carries a build timestamp, which says nothing useful. Name it by what it is:
    # the tag it belongs to and the commit it came from.
    asset = os.path.join(APP_DIR, "lxcontrol-%s-Win64-%s.zip" % (tag, sha))
    if os.path.exists(asset):
        os.remove(asset)
    os.rename(produced[0], asset)
    print("artifact: %s (%.1f MB)" % (os.path.basename(asset), os.path.getsize(asset) / 1e6))

    exists = subprocess.run(["gh", "release", "view", tag], cwd=APP_DIR,
                            capture_output=True).returncode == 0
    if exists:
        print("release %s exists -- replacing its asset" % tag)
        run(["gh", "release", "upload", tag, asset, "--clobber"], cwd=APP_DIR)
        if args.publish:
            run(["gh", "release", "edit", tag, "--draft=false"], cwd=APP_DIR)
    else:
        # Target the commit, not the branch name: in CI the checkout is detached, so --abbrev-ref would
        # hand gh the literal string "HEAD".
        cmd = ["gh", "release", "create", tag, asset,
               "--title", "lxcontrol %s" % tag,
               "--notes", args.notes or "Windows x64 build from %s." % sha,
               "--target", git("rev-parse", "HEAD")]
        if not args.publish:
            cmd.append("--draft")
        run(cmd, cwd=APP_DIR)

    print("\nDone. %s" % ("published" if args.publish else "left as a DRAFT -- review it, then "
          "either publish from the web UI or re-run with --publish"))


if __name__ == "__main__":
    main()
