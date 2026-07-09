---
name: nap-build-run-verify
description: Use when building, regenerating, running, or packaging a NAP app, when deciding whether a change needs regenerate.bat vs build.bat, or when a NAP build/startup fails (LNK1104, "Unknown object type", "Encountered embedded pointer", unresolved symbol, module load errors). Covers the build loop and diagnosing failures.
---

# Build, run & verify a NAP app

## Overview

A NAP app has no unit-test harness — **correctness is verified by building and running** and reading
the startup log. The tooling wraps CMake/MSBuild; which command you need depends on what changed.

## Which command

Run from the app directory:

| Command | When |
|---------|------|
| `regenerate.bat` | After changing `RequiredModules`, **or adding/removing any `.cpp`/`.h` in the module**. Regenerates the VS project's file list. |
| `build.bat` | To compile after any code change. **Close Napkin first** (it locks the module DLL). |
| `package.bat` | To produce a distributable build. |

**The trap:** `build.bat` alone after adding a new `.cpp` reports success while never compiling the
new file (the file list was fixed at the last `regenerate.bat`). The class then fails to load with
`Unknown object type`. **New source file → `regenerate.bat` first, always.** JSON-only edits need
neither — the data json is read at runtime.

## Run & read the load log

Launch the built app (`bin/Release-x86_64/<app>.exe`, or from the IDE). NAP loads the data json
through the `ResourceManager` at startup and **aborts on the first graph error**, printing the exact
message. A clean start shows no load errors and the window appears. That first error line is your
diagnosis — feed it to the doctor.

## Diagnose a failure

Pipe or paste the error into the doctor script — it maps known NAP signatures to cause + fix:

```bash
python ../nap-skill-authoring/scripts/nap_doctor.py --text "LNK1104: cannot open file 'nap<app>.dll'"
build.bat 2>&1 | python ../nap-skill-authoring/scripts/nap_doctor.py
python ../nap-skill-authoring/scripts/nap_doctor.py --list   # every signature it knows
```

It covers the Napkin DLL lock (LNK1104), `Unknown object type`, `Encountered embedded pointer`,
unresolved symbols, module-load and file-not-found failures, duplicate mIDs, and dangling references.

## Verify order for a typical change

1. Edited a data json only → `nap_validate.py data/objects.json`, then run.
2. Added/changed a module class → `regenerate.bat` (if new file) → `build.bat` (Napkin closed) → run.
3. On failure → `nap_doctor.py` → apply fix → repeat. Load errors that trace to the json →
   `nap-resource-graph`; new/way-off class registration → `nap-adding-a-module-class`.

## Common mistakes

- **`build.bat` without `regenerate.bat`** after a new `.cpp` — silent no-compile → `Unknown object type`.
- **Napkin open during build** — `LNK1104: cannot open file …dll`. Close the project in Napkin.
- **Assuming a clean build means it works** — it only means it compiled; run it and read the load log.
