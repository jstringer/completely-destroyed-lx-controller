# NAP parameter types & JSON reference (verified, NAP 0.8.0)

Types from `<SDK>/system_modules/napparameter/include`; blocks from `<SDK>/demos/computeflocking`.
Every `Parameter*` also has `mID` and a display-only `Name`.

## Type family

| JSON `Type` | Family / header | Value properties |
|-------------|-----------------|------------------|
| `nap::ParameterFloat` / `ParameterInt` / `ParameterUInt` / `ParameterByte` / `ParameterDouble` / `ParameterLong` | numeric (`parameternumeric.h`) | `Value`, `Minimum`, `Maximum` — **always clamps** to range |
| `nap::ParameterBool` / `ParameterString` | simple (`parametersimple.h`) | `Value` only |
| `nap::ParameterVec2/Vec3/Vec4` / `ParameterIVec2/IVec3/IVec4` | vector (`parametervec.h`) | `Value {x,y,…}`, `Clamp` (bool, default false), `Minimum`, `Maximum` (scalars) — clamps **only if `Clamp`** |
| `nap::ParameterRGBColorFloat` / `ParameterRGBAColorFloat` / `ParameterRGBColor8` / `ParameterRGBAColor8` | color (`parametercolor.h`) | `Value { "Values": [...] }` |
| `nap::ParameterMat3` / `ParameterMat4` | matrix (`parametermat.h`) | `Value` |
| `nap::ParameterQuat` | quat (`parameterquat.h`) | `Value` |
| `nap::ParameterEnum<T>` | enum (`parameterenum.h`) | `Value` — **needs manual RTTI** (`RTTI_BEGIN_ENUM` + `DEFINE_ENUM_PARAMETER`); no built-in typedef |
| `nap::ParameterButton` | button (`parameterbutton.h`) | none (signals `click`/`press`/`release`); JSON is just `Type`/`mID`/`Name` |
| `nap::ParameterDropDown` | dropdown (`parameterdropdown.h`) | `Items` (string list), `SelectedIndex` (int) |

## Real blocks (from `computeflocking.json`)

```json
{ "Type": "nap::ParameterFloat", "mID": "BoidSizeParameter", "Name": "BoidSize",
  "Value": 0.1, "Minimum": 0.0, "Maximum": 4.0 }

{ "Type": "nap::ParameterInt", "mID": "MeshIndexParameter", "Name": "MeshIndex",
  "Value": 0, "Minimum": 0, "Maximum": 2 }

{ "Type": "nap::ParameterBool", "mID": "ShowBoundsParameter", "Name": "Show Bounds", "Value": false }

{ "Type": "nap::ParameterVec3", "mID": "LightPositionParameter", "Name": "LightPosition",
  "Value": { "x": 0.0, "y": 0.0, "z": 100.0 }, "Clamp": false, "Minimum": -10.0, "Maximum": 10.0 }

{ "Type": "nap::ParameterRGBColorFloat", "mID": "DiffuseColorParameter", "Name": "DiffuseColor",
  "Value": { "Values": [ 0.16, 0.35, 1.0 ] } }
```

Note the value-shape difference: **vec** → `{ "x","y","z" }`; **color** → `{ "Values": [...] }`.

## ParameterGroup (Embedded nesting)

`ParameterGroup` = `Group<Parameter>`. It renames the generic group's `Members`/`Children` to
**`Parameters`/`Groups`** (`parametergroup.h:19-20`), and both are registered
`Embedded | ReadOnly` (`<SDK>/include/nap/group.h:221-229`) — so they hold **inline objects**.

`computeflocking.json:833-931` (root group empty of direct params, holding child groups):
```json
{
    "Type": "nap::ParameterGroup", "mID": "Parameters",
    "Parameters": [],
    "Groups": [
        { "Type": "nap::ParameterGroup", "mID": "Flocking",
          "Parameters": [
              { "Type": "nap::ParameterInt", "mID": "MeshIndexParameter", "Name": "MeshIndex",
                "Value": 0, "Minimum": 0, "Maximum": 2 }
          ],
          "Groups": [] }
    ]
}
```
A component elsewhere references an embedded parameter by its `mID` (e.g. `"BoidSize": "BoidSizeParameter"`).

## ParameterGUI

`computeflocking.json:826-831`:
```json
{ "Type": "nap::ParameterGUI", "mID": "ParameterGUI", "Serializable": true, "Group": "Parameters" }
```
- `Group` — a **Default** id reference to the `ParameterGroup` to show (incl. child groups).
- `Serializable` (default true) — whether this group is a preset that can be saved/loaded via the GUI.
- C++: `void show(bool newWindow = true)` (`parametergui.h:40`) — call every frame in `update()` after
  `mGuiService->selectWindow(window)`. Demos pass `show(false)` to embed into their own `ImGui::Begin`
  block (`<SDK>/demos/linemorphing/src/linemorphingapp.cpp:79-93`).

## Presets via ParameterService (`parameterservice.h`)

- `PresetFileList getPresets(const ParameterGroup& group) const;`
- `bool loadPreset(ParameterGroup& group, const std::string& presetFile, ErrorState&);`
- `bool savePreset(ParameterGroup& group, const std::string& presetFile, ErrorState&);`
- `std::string getPresetPath(const std::string& groupID, const std::string& filename) const;`
- `std::string getGroupPresetDirectory(const std::string& groupID) const;`
- Signals `presetLoaded`, `fileLoaded`.

Storage: `ParameterServiceConfiguration.PresetsDirectory` (default `"Presets"`); a preset file lives
at `<PresetsDirectory>/<groupID>/<filename>.json` and is itself serialized ParameterGroup JSON. Fetch
the service with `getCore().getService<nap::ParameterService>()`; in practice the `ParameterGUI`
popup drives save/load for you.

## Required modules

`napparameter`, `napparametergui`.
