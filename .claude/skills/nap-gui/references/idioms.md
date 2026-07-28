# NAP GUI idioms (patterns pulled from this SDK's demos)

Practical, intended usage patterns, each lifted from a demo in `../../demos/` and cited to
`file:line`. Copy the shape, not the specifics. All are ImGui 1.76 (see `gui-api.md`). The rule from
`SKILL.md` holds throughout: **all of this runs in `update()`** (or a helper it calls); `render()`
only calls `mGuiService->draw()`.

---

## 1. Factor the GUI into per-panel methods called from `update()`

Don't pile every widget inline in `update()`. Give each logical panel its own method and call them in
sequence. Each panel is one `Begin`/`End`, positioned once with `ImGuiCond_Once` so the user can drag
it afterward and it won't snap back.

```cpp
// artnetreceiveapp.cpp:131-147
void ArtNetReceiveApp::showGeneralInfo()
{
    ImGui::SetNextWindowPos(ImVec2(32, 32),  ImGuiCond_Once);   // Once = initial only; user can move it
    ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_Once);   // height 0 = auto-fit to content
    ImGui::Begin("Art-Net Receiving Demo");
    ImGui::Text(getCurrentDateTime().toString().c_str());
    ImGui::Text(utility::stringFormat("Framerate: %.02f", getCore().getFramerate()).c_str());
    ImGui::Separator();
    ImGui::TextWrapped("Use Napkin to change the Net, SubNet and Universe.");
    ImGui::End();
}
```
`update()` then reads: `mGuiService->selectWindow(mRenderWindow); showGeneralInfo(); showReceivedArtnet();`
Demos doing this: `artnetreceive`, `copystamp` (`updateGui()`), `multiwindow` (`updateGUI()`),
`heightmap`, `videomodulation`. Positions/sizes are raw pixels — the service applies the global GUI
scale itself, so don't multiply them by `getScale()`.

---

## 2. Bind a widget straight to live state (the immediate-mode payoff)

Pass the address of the value you want edited; the widget mutates it in place the frame the user
drags. No change-callbacks, no apply button.

```cpp
// copystampapp.cpp:192-199
RenderableCopyMeshComponentInstance& copy_comp = mWorldEntity->getComponent<RenderableCopyMeshComponentInstance>();
if (ImGui::CollapsingHeader("Controls"))
{
    ImGui::Checkbox("Look At Camera", &copy_comp.mOrient);
    ImGui::SliderInt("Random Seed",   &copy_comp.mSeed, 0, 100);
    ImGui::SliderFloat("Global Scale",&copy_comp.mScale, 0.0f, 2.0f);
}
```
The value-returning form — `if (ImGui::SliderFloat(...)) { onlyWhenChanged(); }` — is for when a change
needs a side effect (push to a shader uniform, resnapshot, etc.), as in `helloworld` pushing colors to
a UBO. Use the direct-bind form when the owner reads the member every frame anyway.

---

## 3. Live data visualization with `PlotHistogram` / `PlotLines`

For a bar/line view over a contiguous `float` buffer (DMX channels, an FFT, an audio envelope). Pass
the data pointer, count, value range, and a graph size.

```cpp
// artnetreceiveapp.cpp:169-184 — plot Art-Net/DMX channels in groups
while (offset < total_count)
{
    int32_t count = std::min(mGroupSize, total_count - offset);
    std::string label = utility::stringFormat("Channels %.3d-%.3d", offset + 1, offset + count);
    ImGui::PlotHistogram(label.c_str(), received_data.data() + offset, count,
                         0, NULL, 0.0f, 255.0f, ImVec2(0, 64));   // min 0, max 255 (a DMX byte), 64px tall
    offset += count;
}
```
`audioanalysis`/`audiovisualfft` use the same call for an FFT magnitude buffer. This is the idiomatic
way to show a rig's live output in-app — directly relevant to a DMX/Art-Net controller.

---

## 4. Modal confirmation / notice dialog

`OpenPopup(id)` arms it; `BeginPopupModal(id, …)` draws it while open and blocks the rest of the GUI;
`CloseCurrentPopup()` dismisses. A `static bool` latch opens it exactly once. Use for
destructive-action confirms ("Delete this preset?") and one-time notices.

```cpp
// copystampapp.cpp — open once (:180-186), then draw (:204-217)
static bool popup_opened = false;
if (!popup_opened) { ImGui::OpenPopup(popupID); popup_opened = true; }   // popupID is a static const char*
...
void CopystampApp::handlePopup()
{
    if (ImGui::BeginPopupModal(popupID, nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Performance is ~100x better in a release build");
        if (ImGui::ImageButton(mGuiService->getIcon(icon::ok), "Gotcha"))   // NAP icon button
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
```
`OpenPopup` and the matching `BeginPopupModal` must use the **same string id**, and both must be
reached in the same window scope. Also in `audioplayback`, `licensecheck`, `paintobject`.

---

## 5. Package a self-contained tool as its own GUI class

When a control surface has its own state (selection indices, cached lists) and you want it reusable
across apps, make it a small class holding that state with a single `drawGui()` method — not a pile of
app members. This is how NAP ships its runtime audio-device picker.

```cpp
// audiodevicesettingsgui.h:19-33 — a reusable GUI widget with internal state
class AudioDeviceSettingsGui
{
public:
    AudioDeviceSettingsGui(PortAudioService& service, bool showInputs = true);
    void drawGui();                 // the app just calls this each frame inside its own window
private:
    int mDriverSelection = 0;       // widget state lives with the widget, not the app
    int mInputDeviceSelection = 0;
    std::vector<int> mBufferSizes = { 64, 128, 256, 512, 1024, 2048 };
    // ...
};
```
The owning app holds one instance and calls `mSettingsGui.drawGui()` inside a `Begin`/`End`. Same
principle as NAP's own `ParameterGUI` and `SequenceEditorGUI` — a GUI object with a `show()`/`draw()`
call (`nap-parameters`, `nap-sequence`). Reach for this once a panel's state outgrows a few app members.

---

## 6. Loop-safe widgets: `PushID` per iteration

Widgets are keyed by label; identical labels in a loop collide (they share state). Wrap each iteration
in a unique id. lxcontrol's Fixtures tab (in-repo example) does exactly this:

```cpp
// lxcontrol src/lxcontrolapp.cpp — per-fixture, per-channel controls in a loop
for (size_t i = 0; i < params.size(); ++i)
{
    ImGui::PushID(static_cast<int>(i));      // distinct id even when labels repeat
    ImGui::SliderFloat(name.c_str(), &v, r->mMinimum, r->mMaximum);
    ImGui::PopID();                          // always balance
}
```
Pass a pointer (`PushID(ptr)`) or an int; balance every `PushID` with a `PopID`.

---

## Quick idiom → demo index

| Need | Idiom | Source |
|---|---|---|
| Multi-panel control app | §1 per-panel methods | `artnetreceive`, `copystamp`, `multiwindow` |
| Edit live values | §2 direct-bind widgets | `copystamp`, `helloworld` |
| Show live rig/audio data | §3 `PlotHistogram` | `artnetreceive`, `audioanalysis` |
| Confirm / notice dialog | §4 modal popup | `copystamp`, `audioplayback`, `paintobject` |
| Reusable stateful tool | §5 GUI class | `audioplayback` (`AudioDeviceSettingsGui`) |
| Widgets in a loop | §6 `PushID`/`PopID` | lxcontrol (in-repo) |
| Tabbed single-window layout | `BeginTabBar`/`BeginTabItem` | lxcontrol `drawMainUI()` (in-repo) |
| Parameter controls / presets | `ParameterGUI::show()` | → `nap-parameters` |
| Sequencer timeline | `SequenceEditorGUI::show()` | → `nap-sequence` |
