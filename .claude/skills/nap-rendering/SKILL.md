---
name: nap-rendering
description: Use when rendering with NAP's naprender module — setting up a RenderWindow, camera, mesh, Material, Shader, or RenderableMeshComponent in a data json; writing an App::render() that draws the scene; wiring uniforms/samplers; choosing BlendMode/DepthMode; or debugging "nothing draws". Covers the draw loop and the material/shader/mesh graph.
---

# NAP rendering (naprender)

## Overview

NAP draws by putting a **mesh + material** on an entity via a `RenderableMeshComponent`, then having
`App::render()` gather the `RenderableComponentInstance`s and hand them to
`RenderService::renderObjects()` — inside a strict frame/recording sandwich. The declarative half
(window, camera, mesh, material, shader) lives in the data json; the imperative half (the draw
sequence) lives in C++. This is a `nap-resource-graph` specialization plus a render loop.

## When to use

- Adding a `RenderWindow`, camera (`PerspCameraComponent`/`OrthoCameraComponent`), mesh, `Material`,
  `Shader`, or `RenderableMeshComponent` to a data json.
- Writing/fixing `App::render()`, or setting uniform/sampler values each frame.
- "My mesh doesn't draw", blend/transparency issues, or picking `BlendMode`/`DepthMode`.

## Mental model

- **Resources** (json): a `Mesh` (`BoxMesh`, `SphereMesh`, `MeshFromFile`, …), a `Shader`
  (`ShaderFromFile` with `VertShader`/`FragShader`, or a built-in code shader like `ConstantShader`),
  and a `Material` binding a shader + uniform/sampler values.
- **On an entity**: `RenderableMeshComponent` (Mesh + MaterialInstance) **plus a required
  `TransformComponent`**, and a camera component on a camera entity. A `RenderWindow` is a top-level
  resource; a `Scene` spawns the entities (see `nap-resource-graph`).
- **In C++**: `App::render()` draws; `App::update()` sets values and builds GUI — never draw calls.

## The draw loop (canonical shape)

```cpp
mRenderService->beginFrame();
if (mRenderService->beginRecording(*mRenderWindow)) {
    mRenderWindow->beginRendering();
    auto& cam = mCameraEntity->getComponent<PerspCameraComponentInstance>();
    std::vector<nap::RenderableComponentInstance*> comps { &mObjEntity->getComponent<RenderableComponentInstance>() };
    mRenderService->renderObjects(*mRenderWindow, cam, comps);   // or the no-list overload = whole scene
    mGuiService->draw();
    mRenderWindow->endRendering();
    mRenderService->endRecording();
}
mRenderService->endFrame();
```

**Rule:** every draw call sits inside `beginFrame`/`endFrame` **and** a successful
`beginRecording`/`endRecording` **and** `beginRendering`/`endRendering`. Set uniforms in `update()`
(`renderer.getMaterialInstance().getOrCreateUniform("UBO")->…->setValue(...)`), not in `render()`.

## Material / shader / mesh / camera JSON

Don't hand-write these from memory — the nested uniform/sampler structure and the enum strings are
exact. Copy a working block: `python ../nap-skill-authoring/scripts/nap_usage.py nap::Material`
(likewise `nap::RenderWindow`, `nap::PerspCameraComponent`, `nap::BoxMesh`). Full blocks, the mesh
table, and every enum value are in **`references/rendering-json.md`**.

## Common mistakes

- **Nothing draws** — the component isn't in the render list (or the scene), its camera is
  incompatible, or `Visible` is false; `renderObjects` silently omits such components. Also check the
  entity has a `TransformComponent` (required by `RenderableMeshComponent`).
- **Uniform/sampler `Name` ≠ shader identifier** — `"Name"` must equal the GLSL `uniform`/`sampler`
  name (and the UBO struct name), not the `mID`. Vertex attributes are matched via
  `VertexAttributeBindings`.
- **`ShaderFromFile` vs code shader** — `ShaderFromFile` needs `VertShader`+`FragShader` paths; a
  code shader (`ConstantShader`, `GnomonShader`, …) is just `Type`+`mID` (no files).
- **`MaterialInstance` BlendMode/DepthMode `NotSet`** means "inherit from the Material"; the
  `Material` itself must resolve to a concrete mode (defaults `Opaque` / `InheritFromBlendMode`).
- **Wrong projection matrix** — feed shaders `getRenderProjectionMatrix()` (Vulkan clip space), not
  `getProjectionMatrix()`, or geometry flips vertically.

## Verify

Lint the json (`nap-validate-data-json`), then build+run and read the load log
(`nap-build-run-verify`). Worked example: `<SDK>/demos/rotatingtexcube` (one window/camera/textured
box — cleanest end-to-end); `<SDK>/demos/helloworld` for two cameras + sphere + explicit attribute
bindings.
