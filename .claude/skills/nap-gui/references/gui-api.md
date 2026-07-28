# NAP GUI API reference (napimgui)

Heavy detail for `nap-gui/SKILL.md`. Everything here is grounded in this SDK's `napimgui` headers
(`../../system_modules/napimgui/include/`) and demo usage; line cites are to those files. Stock ImGui
widgets are **ImGui 1.76** (`imgui/imgui.h`, `IMGUI_VERSION_NUM 17600`) — grep that header for the
exact signature/flag before writing it.

## IMGuiService (imguiservice.h)

Fetched in `App::init()` via `getCore().getService<nap::IMGuiService>()`. Methods you actually call:

| Method | Purpose | Cite |
|---|---|---|
| `selectWindow(ResourcePtr<RenderWindow>)` | Route subsequent `ImGui::` calls to this window. Call in `update()`. Mandatory with >1 window and before any `*GUI::show()`. | `imguiservice.h:131-149` |
| `draw()` | Blit the current window's GUI. Call in `render()`, inside `beginRecording`/`beginRendering`. | `imguiservice.h:100-129` |
| `getPalette()` → `const gui::ColorPalette&` | Active theme colors (see palette table). | `imguiservice.h:182-186` |
| `setPalette(gui::EColorScheme)` | Switch color scheme at runtime. | `imguiservice.h:188-192` |
| `getScale()` / `getScale(ctx)` → `float` | Current DPI×global GUI scale. Widget sizes are already scaled by the service; use this only if you need the factor. | `imguiservice.h:165-180` |
| `getTextureHandle(const Texture2D&)` → `ImTextureID` | Handle for drawing a texture in a window draw-list. Cached per texture; fine to call every frame. | `imguiservice.h:217-230` |
| `getIcon(std::string&&)` → `nap::Icon&` | Fetch a loaded icon by name (use the `nap::icon::` constants). | `imguiservice.h:232-245` |
| `loadIcon(name, module, errorState)` → `bool` | Register a custom icon from a module's data search path. Call at service/app init; names must be unique. | `imguiservice.h:247-269` |
| `getContext(window)` / `findContext(id)` | The window's `ImGuiContext*` (advanced/interop). | `imguiservice.h:151-163` |

Comment on the class itself: *"The service automatically creates a new GUI frame before application
update"* — so `update()` is where a valid frame exists to add widgets to (`imguiservice.h:86-94`).

## NAP additions to the `ImGui::` namespace (imguiutils.h)

These live in `namespace ImGui` and follow ImGui naming, so they read like stock calls:

| Call | Does | Cite |
|---|---|---|
| `ImGui::Image(const nap::Texture2D&, size, uv0, uv1, tint, border)` | Draw a NAP texture as an image. Note NAP default `uv0=(0,1)`, `uv1=(1,0)` (flipped V). | `imguiutils.h:32` |
| `ImGui::ImageButton(const nap::Texture2D&, size, …)` | Texture as a button; returns `bool` pressed. | `imguiutils.h:45` |
| `ImGui::ImageButton(const nap::Icon&, text=nullptr, …)` | Icon button, height = frame height; hover shows name/`text`. | `imguiutils.h:64` |
| `ImGui::ImageButton(const nap::Icon&, size, text, …)` | Icon button at explicit size. | `imguiutils.h:84` |
| `ImGui::GetTextureHandle(nap::Texture2D&)` → `ImTextureID` | Handle for manual draw-list use. | `imguiutils.h:91` |
| `ImGui::Combo(label, int* item, nap::rtti::TypeInfo enum_type)` | Combo box auto-populated from an RTTI enum: `ImGui::Combo("Mode", &i, RTTI_OF(nap::EMyEnum))`. Asserts if the type isn't an enum. | `imguiutils.h:93-109` |

## Icons (imguiservice.h:39-64, imguiicon.h)

Built-in icon name constants in `namespace nap::icon` — guaranteed to exist:
`save, saveAs, cancel, ok, del, file, help, settings, reload, folder, load, info, warning, error,
copy, paste, insert, edit, remove, add, change, subtract, frame`.

```cpp
if (ImGui::ImageButton(mGuiService->getIcon(nap::icon::save)))
    doSave();
```
`nap::Icon` is a `Resource` (`imguiicon.h:24`): `ImagePath` (property), `Invert` (property),
`getTexture()`, `getTextureHandle()`, `getName()`. Custom icons register through
`IMGuiService::loadIcon()` and are then fetched by name via `getIcon()`.

## Theming: IMGuiServiceConfiguration (imguiservice.h:70-83)

A `ServiceConfiguration` — set it once in the app's **service-config JSON** (same file that holds
`nap::RenderServiceConfiguration` etc.), not in `objects.json`. Shape mirrors any config block
(cf. `../../demos/audioplayback/config.json`):

```json
{
    "Objects": [
        {
            "Type": "nap::IMGuiServiceConfiguration",
            "mID": "nap::IMGuiServiceConfiguration",
            "Color Scheme": "Dark",
            "Font Size": 17.0,
            "Scale": 1.0,
            "FontFile": ""
        }
    ]
}
```
Properties (defaults from the header): `Color Scheme` (Dark), `Font Size` (17.0), `Scale` (1.0),
`FontFile` (""; empty = bundled NAP font), `FontSampling` ({5,3}), `FontSpacing` (0.25), `Colors`
(a `gui::ColorPalette`, used when scheme is `Custom`), `Style` (a `gui::Style`).
**Confirm the exact serialized property strings and the `EColorScheme` string against a working
config block before trusting them** — enum strings and the Embedded/Default shape of the nested
`Colors`/`Style` are not carried by the header (see
`../nap-skill-authoring/references/authoritative-sources.md`).

`EColorScheme` (identifiers — `imguistyle.h:26-33`): `Light`, `Dark`, `HyperDark`, `Classic`, `Custom`.

### ColorPalette (imguistyle.h:38-53) — `RGBColor8` fields

`BackgroundColor, DarkColor, MenuColor, FrontColor1..4` (Front4 = text), `HighlightColor1`
(selection), `HighlightColor2` (info), `HighlightColor3` (warning), `HighlightColor4` (errors),
`InvertIcon`. To use in a widget, convert to float first:

```cpp
const auto& theme = mGuiService->getPalette();
ImGui::TextColored(theme.mHighlightColor4.convert<RGBColorFloat>(), "error!");   // multiwindow:357
```

### Style (imguistyle.h:58-81)

Serializable spacing/rounding knobs mapped onto `ImGuiStyle`: `WindowPadding, WindowRounding,
FramePadding, FrameRounding, ItemSpacing, ItemInnerSpacing, IndentSpacing, ScrollbarSize,
ScrollbarRounding, GrabMinSize, GrabRounding, WindowBorderSize, PopupRounding, ChildRounding,
WindowTitleAlign, PopupBorderSize, TabRounding, TouchExtraPadding, AntiAliasedLines, AntiAliasedFill`.

## Event handling (guiappeventhandler.h)

`nap::GUIAppEventHandler` — set as the `AppRunner`'s second template arg in `main.cpp`. It builds an
`SDLEventConverter`, forwards input to ImGui, and **withholds** events from the app when a widget is
active (`guiappeventhandler.h:16-37`). `setTouchGeneratesMouseEvents(bool)` decouples touch from
mouse if you want raw touch to reach the GUI (`:44-52`). Without this handler the GUI never receives
input. In-app you can ask `IMGuiService::isCapturingMouse(ctx)` / `isCapturingKeyboard(ctx)` whether
the GUI currently owns input (`imguiservice.h:200-208`).

## Common stock 1.76 widgets (verify signature in imgui.h before use)

Immediate-mode: value-editing widgets return `true` the frame the value changed; buttons return
`true` the frame clicked. Layout: `SameLine`, `Separator`, `Columns`, `BeginChild/EndChild`,
`PushID/PopID`. Containers seen in this SDK's apps: `Begin/End`, `BeginTabBar/EndTabBar` +
`BeginTabItem/EndTabItem`, `CollapsingHeader`, `TreeNode`. Inputs: `Button`, `Checkbox`,
`SliderFloat`, `InputText`, `ColorEdit3`, `Combo`, `Text`/`TextColored`/`TextDisabled`. There is **no
Tables API and no docking** in 1.76 — use `Columns()` or child regions for grids/panes. Grep the
header for anything else:

```bash
grep -n "IMGUI_API" ../../system_modules/napimgui/include/imgui/imgui.h | grep -i "sliderfloat"
```
