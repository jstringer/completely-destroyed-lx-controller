---
name: lx-drive
description: Use when you need to see or navigate lxcontrol's running UI — screenshotting the app window, switching tabs/mode, clicking a button by label, or reading back on-screen state to iterate on GUI/UX. Triggers on "screenshot the app", "show me the UI", "click X", "switch to PROGRAMS", "does this layout look right".
---

# Driving the lxcontrol UI

Playwright-shaped loop for an ImGui app: **screenshot → read labels → click by label → screenshot**.

`nap::Snapshot` is NOT the tool for this. It renders `RenderableComponent`s through a `PerspCamera`
into a headless target and deliberately excludes GUI — it would capture the gnomon. Pixels come from
Win32 window capture instead; control comes from the in-app bridge in `src/lxagent.h`.

## Driver

```
pwsh -NoProfile -File .claude/skills/lx-drive/scripts/lxui.ps1 <verb> [arg]
```

| Verb | Effect |
|---|---|
| `state` | ack only: mode, active tab, program/cued, voice count, every clickable label |
| `shot -Out <path.png>` | PNG of the app window (no app round-trip) |
| `tab RIG\|PROGRAMS\|CONTROLS` | select an authoring tab |
| `mode edit\|perform` | Live Bar Perform/Edit toggle |
| `click "<label>"` | click a button by its label, e.g. `click "+ New Patch"` |
| `text patch\|control\|device\|group\|program <value>` | fill a creation-form text buffer (`device` = a Control's device label, `group` = a FixtureGroup name) |
| `styleguide on\|off` | Style Guide test-bed window |
| `resize "<w> <h>"` | resize the render window |
| `stopall` | `lxcontrolService::stopAll()` |

## The loop

1. **App must be running.** `bin/Release-x86_64/lxcontrol.exe` — launch with `run_in_background`.
2. `lxui.ps1 state` → read `CLICKABLE:` for the exact keys addressable *this frame*.
3. `lxui.ps1 click "…"` → the ack reports `HIT` or `MISS`, plus the new state.
4. `lxui.ps1 shot -Out <scratchpad>/ui.png` → `Read` the PNG (Read renders images inline).

Keep screenshots in the session scratchpad, not the repo.

## Rules that actually matter

- **One command per frame.** Each invocation writes `.agent/cmd` and blocks on `.agent/ack`. Never
  batch two verbs into one file.
- **A button only exists while its tab is drawing.** `click` on a widget in another tab returns
  `MISS`. Send `tab X` first, as a separate command.
- **Address the folded key, exactly as `CLICKABLE` prints it** — copy it, don't retype it from the
  screenshot. Keys drop the `##id` suffix (`Del##trig` → `Del`), collapse whitespace runs to one
  space (so the two-line Perform pads are addressable: `click "button1 green_down [3 fx]"`), and drop
  non-ASCII bytes, which would not survive the shell round-trip anyway. Duplicates within a frame get
  an occurrence suffix: `[Fire]`, `[Fire#2]`, `[Fire#3]`. Note the top bar's All Stop is spelled
  `# All Stop` in code — quote it as-is.
- **Clicks are instantaneous — nothing can be held.** `fireTrigger` is called with `held = false`, so
  a Momentary/Hold control stabs. `VOICES` counts only held, non-releasing activations
  (`activeVoiceCount`), so it stays `0` after a bridge click even though the trigger really fired —
  confirm firing from the pad going gold in a screenshot, not from the voice count.
- **Only buttons and tabs are addressable.** Checkboxes, sliders, combos and the routing matrix are
  read-only to the bridge — for those, ask the user to click, or add a verb to
  `lxcontrolApp::handleAgentCommand`.
- **Capture needs the window on screen.** `PrintWindow(PW_RENDERFULLCONTENT)` is tried first; if the
  frame comes back black (common for Vulkan swapchains) the script raises the window and grabs the
  screen, which steals focus for ~250ms. A locked screen or minimized window yields garbage.
- **`MISS` with no state change** usually means the app isn't polling: check the log line
  `lxagent bridge: <abs path>` printed on the first frame and compare it with the script's `-Dir`
  (default `<app>/.agent`; override with `$env:LXUI_DIR`).

## Extending it

A new addressable action is two lines: draw it with `lxagent::Button` (already the default for every
button in `lxcontrolapp.cpp` and `lxtheme::DangerButton`), or add a verb to
`lxcontrolApp::handleAgentCommand` in `src/lxcontrolapp.cpp` and call `lxagent::hit()` in it.
`src/lxagent.h` is header-only, so no `regenerate.bat` is needed for either.
