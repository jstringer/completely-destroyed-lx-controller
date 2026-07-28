---
name: nap-gui
description: Use when building any GUI, control panel, HUD, editor, or developer/debug UI in a NAP app with the napimgui module — drawing ImGui widgets, wiring GUIAppEventHandler, selectWindow/draw across the update/render split, theming via IMGuiServiceConfiguration, showing NAP textures/icons/enums in a GUI, or debugging "GUI doesn't receive input" / "ImGui call had no effect" / a napimgui.dll crash. Covers NAP's ImGui integration idioms and the ImGui 1.76 version constraint.
---

# NAP GUI (napimgui)

## Overview

NAP embeds **Dear ImGui** (immediate-mode GUI) via the `napimgui` module. You do not declare
widgets in JSON — you call `ImGui::*` from C++ every frame. NAP owns the ImGui context, the render
pass, input routing, and theming; you own the widget calls. The whole surface is standard ImGui
**plus** a thin set of NAP helpers for showing NAP objects (textures, icons, RTTI enums). Get the
lifecycle placement and the event handler right and everything else is ordinary ImGui.

## When to use

- Drawing any control panel / HUD / debug overlay / in-app editor tool in a NAP app.
- Wiring up a new app so its GUI actually receives mouse/keyboard input.
- Theming (colors, font, scale), high-DPI, multi-window GUI, or showing a NAP `Texture2D`/`Icon`/enum.
- Debugging: GUI draws but ignores input; an `ImGui::` call seems to do nothing; a crash inside
  `napimgui.dll`; a modern ImGui API (`BeginTable`, docking) won't compile.
- **When NOT to use:** rendering `ParameterFloat`/`ParameterGroup` controls or preset save/load →
  `nap-parameters` (`ParameterGUI`). A sequencer timeline → `nap-sequence` (`SequenceEditorGUI`).
  Both are ImGui windows that follow the same `selectWindow`-then-`show()` rule described here.

## Mental model — the three non-negotiables

1. **Widget calls in `update()`, `draw()` in `render()`.** All `ImGui::Begin/…/End` go in
   `App::update()` (or a helper it calls). `render()` only calls `mGuiService->draw()` *inside* the
   window's record/render pass to blit what update built. ImGui state built during `render()` is
   drawn a frame late or not at all. (`imguiservice.h:86-94`; `helloworld` update vs render.)

2. **The app must use `GUIAppEventHandler`, or the GUI is dead to input.** It's chosen in
   `main.cpp` as the `AppRunner`'s event handler — it routes SDL input into ImGui and *withholds*
   events from the app while the pointer is over a widget. The default `AppEventHandler` never feeds
   ImGui, so widgets render but never respond. (`guiappeventhandler.h:16-37`; `helloworld/src/main.cpp`.)

3. **`selectWindow()` before the widgets for a window** — mandatory with >1 window, and mandatory
   before `ParameterGUI::show()` / `SequenceEditorGUI::show()` even with one window (skipping it
   crashes inside `napimgui.dll`, it does not fail gracefully). Call it in `update()`, never in
   `render()`. With a single window and plain `ImGui::` calls it's effectively a no-op but harmless.
   (`imguiservice.h:131-149`; `multiwindow::updateGUI()`.)

Requirement to even start: `napimgui` must be in the app's `RequiredModules` in `app.json`
(`nap-build-run-verify` after adding it). `IMGuiService` depends on `RenderService`.

## The canonical wiring (grounded skeleton)

```cpp
// main.cpp — the event handler is the whole reason input works
nap::AppRunner<MyApp, nap::GUIAppEventHandler> app_runner(core);
```
```cpp
// myapp.cpp
void MyApp::update(double deltaTime)
{
    mGuiService->selectWindow(mRenderWindow);        // before any ImGui:: call
    ImGui::Begin("Controls");
    ImGui::Text("Framerate: %.1f", getCore().getFramerate());
    if (ImGui::Button("Go")) { /* fires the frame it's clicked */ }
    ImGui::End();
}

void MyApp::render()
{
    mRenderService->beginFrame();
    if (mRenderService->beginRecording(*mRenderWindow))
    {
        mRenderWindow->beginRendering();
        // ... your scene render calls ...
        mGuiService->draw();                          // GUI last, so it overlays the scene
        mRenderWindow->endRendering();
        mRenderService->endRecording();
    }
    mRenderService->endFrame();
}
```
`mGuiService` is a `nap::IMGuiService*` fetched in `init()` with `getCore().getService<nap::IMGuiService>()`
(see any demo app's `init()`). Immediate mode: buttons/sliders return their result the frame the user
acts; there is no retained widget tree and no callbacks to register.

## The version trap — this is ImGui **1.76**

The bundled ImGui is **1.76** (`imgui.h`, `IMGUI_VERSION_NUM 17600`). APIs you may "remember" from
newer ImGui **do not exist here** and won't compile:
- **No Tables API** (`BeginTable`/`TableNextColumn`, added 1.80) — use `Columns()` or child regions.
- **No docking / viewports** (that's the `docking` branch) — no `DockSpace`, no `ImGuiWindowFlags_*Docking`.
- `ImGui::ShowDemoWindow()` **is** available and is the fastest way to discover which widgets/flags
  the 1.76 API actually offers before you write them (`multiwindow` calls it). When unsure a symbol
  exists, grep the bundled header rather than trusting memory — see below.

## NAP-specific helpers (what napimgui adds on top of stock ImGui)

Detailed signatures, flags, theming JSON, and icon list are in **`references/gui-api.md`**. In short:
- **Show a NAP texture:** `ImGui::Image(const nap::Texture2D&, size, …)` and the `ImageButton`
  overloads — no manual descriptor handling. (`imguiutils.h:32-45`.)
- **Show an RTTI enum as a combo:** `ImGui::Combo(label, &item, RTTI_OF(nap::EMyEnum))` auto-lists
  the registered members. (`imguiutils.h:93-109`.)
- **Icons:** built-in set via `mGuiService->getIcon(nap::icon::save)` → `ImGui::ImageButton(icon)`;
  add your own with `IMGuiService::loadIcon()`. (`imguiservice.h:39-64, 232-269`.)
- **Theme colors for `TextColored` etc.:** `mGuiService->getPalette()` returns an
  `RGBColor8` palette; convert with `.convert<RGBColorFloat>()` before passing to ImGui
  (`multiwindow` line 357). (`imguistyle.h:38-53`.)
- **Theming/font/scale** are set once via an `IMGuiServiceConfiguration` in the app's service-config
  JSON (color scheme Dark/Light/HyperDark/Classic/Custom, `FontSize`, `Scale`, `FontFile`).
  High-DPI scaling is automatic — query `getScale()`; don't hand-multiply widget sizes by DPI.
  (`imguiservice.h:70-83`, `imguistyle.h:26-33`; config-JSON shape in the reference.)

## Finding the exact API

Two different lookups — don't mix them up:
- **Stock ImGui widgets/flags** (`Begin`, `SliderFloat`, `BeginTabBar`, `ImGuiTreeNodeFlags_*`): grep
  the bundled header — it's the authority for *this* 1.76, over any online ImGui docs:
  `grep -n "IMGUI_API .*Slider\|BeginTabBar" ../../system_modules/napimgui/include/imgui/imgui.h`.
- **NAP classes/config** (`IMGuiService`, `IMGuiServiceConfiguration`, `Icon`, `gui::ColorPalette`):
  headers under `../../system_modules/napimgui/include/` (property names + doc-comment usage
  examples), and `nap_doc.py IMGuiService` for the docs URL. Service-config JSON shape: copy a working
  `*ServiceConfiguration` block — full mechanics in
  `../nap-skill-authoring/references/authoritative-sources.md`.

## Common mistakes

- **Default event handler** → GUI renders but ignores clicks. Use `nap::GUIAppEventHandler` in `main.cpp`.
- **`ImGui::` calls in `render()`** → drawn late/never. Build the GUI in `update()`; `render()` only
  calls `draw()`.
- **`draw()` outside the window's record/render pass**, or forgetting it → no GUI on screen.
- **`ParameterGUI::show()` / `SequenceEditorGUI::show()` without a preceding `selectWindow()`** →
  hard crash in `napimgui.dll`, not a graceful error. Both must run in `update()`.
- **Reaching for `BeginTable`/docking** → won't compile; this is 1.76. Use `Columns()` / child windows.
- **Unbalanced stack calls** → misrendered or asserting GUI. Every `Begin`/`BeginChild`/`BeginTabBar`/
  `PushID` needs its matching `End*`/`PopID`, and `BeginTabItem` returning true still needs `EndTabItem`.
- **Reusing labels without IDs** in a loop → widgets share state/collide. Wrap each iteration in
  `ImGui::PushID(i) … PopID()` (lxcontrol's Fixtures tab does this).

## Verify your change

`napimgui` is a system module (headers only, no rebuild of it) — but adding it to `RequiredModules`
needs `regenerate.bat`; a new app `.cpp` that draws GUI needs `regenerate.bat` then `build.bat`
(`nap-build-run-verify`). Run the app: the GUI window should appear, widgets should respond to the
mouse (proves the event handler), and hovering a widget should stop it from also
rotating/panning the scene (proves input withholding). If Napkin holds the project open, `build.bat`
fails with `LNK1104` on the module DLL — close it first.

## Idiomatic patterns

For practical, copy-me patterns pulled from this SDK's demos — multi-panel apps, direct-bind widgets,
live `PlotHistogram` data views, modal confirm dialogs, packaging a stateful tool as a GUI class, and
loop-safe `PushID` — see **`references/idioms.md`** (each with a `file:line` demo cite).

## Worked examples (in this SDK)

- `../../demos/helloworld/src/helloworldapp.cpp` — minimal, correct update/render split + `main.cpp`
  event handler. Start here.
- `../../demos/artnetreceive/src/artnetreceiveapp.cpp` — multi-panel layout + `PlotHistogram` over a
  DMX channel buffer (closest to a lighting/control app).
- `../../demos/multiwindow/src/multiwindowapp.cpp` — per-window `selectWindow` + one `draw()` per
  window recording; palette conversion; `ShowDemoWindow`.
- `../../demos/copystamp/src/copystampapp.cpp` — dedicated `updateGui()`, direct-bind widgets, modal popup.
- **lxcontrol example (in-repo, not the definition of NAP):** `src/lxcontrolapp.cpp` `drawMainUI()` —
  a `BeginTabBar` layout, `BeginChild` columns, and per-item `PushID` in a real control surface.
