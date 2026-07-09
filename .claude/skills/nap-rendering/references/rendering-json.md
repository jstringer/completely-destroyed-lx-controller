# NAP rendering JSON & API reference (verified, NAP 0.8.0)

Blocks copied from demos (provably load). `<SDK>` = SDK root. Get more with
`nap_usage.py <Type>`; resolve docs with `nap_doc.py <Type>`.

## Material (nested uniforms + sampler)

From `<SDK>/demos/rotatingtexcube/data/objects.json:213-255`:

```json
{
    "Type": "nap::Material", "mID": "CubeMaterial",
    "Uniforms": [
        { "Type": "nap::UniformStruct", "mID": "UBO_uni", "Name": "UBO",
          "Uniforms": [
              { "Type": "nap::UniformVec3", "mID": "color_uni", "Name": "color",
                "Value": { "x": 1.0, "y": 1.0, "z": 1.0 } }
          ] }
    ],
    "Samplers": [
        { "Type": "nap::Sampler2D", "mID": "tex_smp", "Name": "inTexture",
          "MinFilter": "Linear", "MaxFilter": "Linear", "MipMapMode": "Linear",
          "AddressModeVertical": "ClampToEdge", "AddressModeHorizontal": "ClampToEdge",
          "MaxLodLevel": 1000, "AnisotropicSamples": "Default", "Texture": "CubeTexture" }
    ],
    "Buffers": [],
    "Shader": "CubeShader",
    "VertexAttributeBindings": [],
    "BlendMode": "Opaque",
    "DepthMode": "InheritFromBlendMode"
}
```

- `"Name"` on a uniform/sampler/struct must match the GLSL identifier (the mID is arbitrary).
- Uniform value `Type`s (`<SDK>/system_modules/naprender/include/uniform.h`): `UniformFloat`,
  `UniformInt`, `UniformUInt`, `UniformVec2/3/4`, `UniformIVec4`, `UniformUVec4`, `UniformMat4`, and
  `…Array` variants. Scalar/vector uniforms use `"Value"`; array uniforms use `"Values"`.
- Explicit vertex bindings (from `helloworld.json:314-356`):
  `"VertexAttributeBindings": [ { "MeshAttributeID": "Position", "ShaderAttributeID": "in_Position" }, … ]`.
- Newer demos also include `"Constants": []` on Material/MaterialInstance; older ones omit it. Both parse.

## Shaders

`ShaderFromFile` (needs file paths) — `rotatingtexcube/data/objects.json:298-303`:
```json
{ "Type": "nap::ShaderFromFile", "mID": "CubeShader",
  "VertShader": "shaders/cube.vert", "FragShader": "shaders/cube.frag" }
```
Built-in **code** shader (no files) — `lightsandshadow/data/lightsandshadow.json:1438`:
```json
{ "Type": "nap::ConstantShader", "mID": "ConstantShader" }
```
`ConstantShader`'s material exposes a `UBO` with `vec3 color; float alpha;`
(`<SDK>/system_modules/naprender/include/constantshader.h:28-34`) — a flat-colored material needing
no `.vert`/`.frag` files.

## Meshes

All predefined meshes derive from `IMesh` and share `Usage` (`"Static"`), `CullMode`, `PolygonMode`,
`Color` (`{ "Values":[r,g,b,a] }`). Type-specific properties:

| Type | Extra properties |
|------|------------------|
| `nap::BoxMesh` | `Size {x,y,z}`, `Position {x,y,z}`, `FlipNormals` |
| `nap::SphereMesh` | `Radius`, `Rings`, `Sectors`, `Position` |
| `nap::PlaneMesh` | `Size {x,y}`, `Position`, `Rows`, `Columns` |
| `nap::TorusMesh` | `Radius`, `TubeRadius`, `Segments`, `TubeSegments`, `AngleOffset` |
| `nap::GnomonMesh` | `Size` (float), `Position` |
| `nap::MeshFromFile` | `Path` (`.mesh` file) |

`BoxMesh` (`rotatingtexcube/data/objects.json:3-27`):
```json
{ "Type": "nap::BoxMesh", "mID": "CubeMesh", "Usage": "Static", "CullMode": "Back",
  "PolygonMode": "Fill", "Size": { "x": 1.0, "y": 1.0, "z": 1.0 },
  "Position": { "x": 0.0, "y": 0.0, "z": 0.0 }, "Color": { "Values": [1.0,1.0,1.0,1.0] } }
```

## Cameras & window

PerspCamera (`rotatingtexcube/data/objects.json:32-48`) — on a camera entity:
```json
{ "Type": "nap::PerspCameraComponent", "mID": "PerspectiveCamera",
  "Properties": { "FieldOfView": 50.0, "NearClippingPlane": 1.0, "FarClippingPlane": 1000.0,
    "GridDimensions": { "x": 1, "y": 1 }, "GridLocation": { "x": 0, "y": 0 } } }
```
OrthoCamera `Properties`: `Mode` (`PixelSpace`/`CorrectAspectRatio`/`Custom`), plane extents, `ClipRect`.

RenderWindow (top-level resource) — `rotatingtexcube/data/objects.json:256-279`:
```json
{ "Type": "nap::RenderWindow", "mID": "Window", "Borderless": false, "Resizable": true,
  "Visible": true, "SampleShading": true, "Title": "app", "Width": 1280, "Height": 720,
  "Mode": "Immediate", "ClearColor": { "Values": [0.0,0.0,0.0,1.0] }, "Samples": "Four",
  "AdditionalSwapImages": 1, "RestoreSize": true, "RestorePosition": true }
```

## Enum string values (from headers; JSON spelling confirmed in demos)

- `EBlendMode` (`materialcommon.h:18`): `NotSet`, `Opaque`, `AlphaBlend`, `Additive`.
- `EDepthMode` (`materialcommon.h:40`): `NotSet`, `InheritFromBlendMode`, `ReadWrite`, `ReadOnly`,
  `WriteOnly`, `NoReadWrite`.
- `ECullMode`: `None`, `Front`, `Back`, `FrontAndBack`. `EPolygonMode`: `Fill`, `Line`, `Point`.
- `RenderWindow.Mode` (`EPresentationMode`): `Immediate`, `Mailbox`, `FIFO_Relaxed`, `FIFO`.
  `Samples` (`ERasterizationSamples`): e.g. `One`, `Four`.

## The render() sequence (verbatim shape)

`<SDK>/demos/rotatingtexcube/src/rotatingtexcubeapp.cpp:79-116`. Order is load-bearing:
`beginFrame` → `beginRecording(window)` → `window->beginRendering()` → `renderObjects(window, cam,
comps)` → `mGuiService->draw()` → `window->endRendering()` → `endRecording()` → `endFrame()`.
`renderObjects` overloads (`renderservice.h:414,435`): with an explicit component vector, or without
(renders every compatible `RenderableComponent` in the scene, depth-sorted).
