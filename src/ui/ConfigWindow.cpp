#include "ConfigWindow.hpp"

#include <SDL_image.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Window.hpp"
#include "../platform/EmbeddedAssets.hpp"
#include "../platform/Platform.hpp"

namespace rd {

namespace {

constexpr int kWindowW = 440;
constexpr int kWindowH = 680;
constexpr size_t kCmdBufSize = 512;

constexpr ImU32 kPurple = IM_COL32(35, 0, 90, 255);
constexpr ImU32 kGold = IM_COL32(181, 168, 123, 255);
constexpr ImU32 kHeaderGold = IM_COL32(254, 223, 119, 255);
constexpr ImU32 kEditBg = IM_COL32(55, 20, 110, 255);

void ApplyRansomStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.WindowBorderSize = 0.0f;
    style.FramePadding = ImVec2(6, 4);
    style.ItemSpacing = ImVec2(6, 6);
    style.ScrollbarSize = 6.0f;
    style.ScrollbarRounding = 3.0f;

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(IM_COL32(255, 255, 255, 255));
    c[ImGuiCol_FrameBg] = ImGui::ColorConvertU32ToFloat4(kEditBg);
    c[ImGuiCol_FrameBgHovered] = ImGui::ColorConvertU32ToFloat4(IM_COL32(70, 30, 130, 255));
    c[ImGuiCol_FrameBgActive] = ImGui::ColorConvertU32ToFloat4(IM_COL32(80, 35, 145, 255));
    c[ImGuiCol_CheckMark] = ImGui::ColorConvertU32ToFloat4(kHeaderGold);
    c[ImGuiCol_Button] = ImGui::ColorConvertU32ToFloat4(kEditBg);
    c[ImGuiCol_ButtonHovered] = ImGui::ColorConvertU32ToFloat4(IM_COL32(80, 35, 145, 255));
    c[ImGuiCol_ButtonActive] = ImGui::ColorConvertU32ToFloat4(IM_COL32(95, 45, 165, 255));
    c[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(kHeaderGold);
    c[ImGuiCol_ChildBg] = ImGui::ColorConvertU32ToFloat4(IM_COL32(0, 0, 0, 40));
    c[ImGuiCol_PopupBg] = ImGui::ColorConvertU32ToFloat4(IM_COL32(30, 10, 60, 245));
    c[ImGuiCol_ModalWindowDimBg] = ImGui::ColorConvertU32ToFloat4(IM_COL32(0, 0, 0, 140));
    c[ImGuiCol_ScrollbarBg] = ImGui::ColorConvertU32ToFloat4(IM_COL32(0, 0, 0, 0));
    c[ImGuiCol_ScrollbarGrab] = ImGui::ColorConvertU32ToFloat4(IM_COL32(254, 223, 119, 130));
    c[ImGuiCol_ScrollbarGrabHovered] = ImGui::ColorConvertU32ToFloat4(IM_COL32(254, 223, 119, 190));
    c[ImGuiCol_ScrollbarGrabActive] = ImGui::ColorConvertU32ToFloat4(IM_COL32(254, 223, 119, 230));
}

void DrawGradientBackground(ImDrawList* draw, ImVec2 pos, ImVec2 size) {
    draw->AddRectFilledMultiColor(pos, ImVec2(pos.x + size.x, pos.y + size.y), kPurple, kPurple, kGold, kGold);
}

void DrawRotatedImage(ImDrawList* draw, ImTextureID tex, ImVec2 center, ImVec2 size, float angleDeg) {
    if (!tex) return;
    float angle = angleDeg * (3.14159265f / 180.0f);
    float cosA = std::cos(angle), sinA = std::sin(angle);
    ImVec2 half(size.x * 0.5f, size.y * 0.5f);
    ImVec2 local[4] = {{-half.x, -half.y}, {half.x, -half.y}, {half.x, half.y}, {-half.x, half.y}};
    ImVec2 corners[4];
    for (int i = 0; i < 4; ++i) {
        corners[i] = ImVec2(center.x + local[i].x * cosA - local[i].y * sinA,
                             center.y + local[i].x * sinA + local[i].y * cosA);
    }
    draw->AddImageQuad(tex, corners[0], corners[1], corners[2], corners[3]);
}

bool BeginSection(const char* title, float height) {
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kHeaderGold), "%s", title);
    return ImGui::BeginChild(title, ImVec2(0, height), true);
}
void EndSection() { ImGui::EndChild(); }

// step=0 drops InputInt's +/- buttons - they clip the label otherwise.
void LabeledInputInt(const char* label, int* v) {
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-1);
    ImGui::PushID(label);
    ImGui::InputInt("##field", v, 0, 0);
    ImGui::PopID();
}

struct FrameResult {
    bool closeRequested = false;
    bool accepted = false;
    bool spawnRequested = false;
};

FrameResult DrawConfigUI(Config& draft, char* cmdBuf, ImTextureID starlightTex) {
    FrameResult result;
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                              ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                              ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("RansomConfig", nullptr, flags);

    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    DrawGradientBackground(draw, winPos, winSize);

    ImGui::SetWindowFontScale(1.5f);
    const char* headerText = "RANS0M Configuration";
    float headerW = ImGui::CalcTextSize(headerText).x;
    ImGui::SetCursorPos(ImVec2((winSize.x - headerW) * 0.5f, 10));
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kHeaderGold), "%s", headerText);
    ImGui::SetWindowFontScale(1.0f);

    DrawRotatedImage(draw, starlightTex, ImVec2(winPos.x + 34, winPos.y + 24), ImVec2(46, 46), 7.721f);

    ImGui::SetCursorPosY(46);
    ImGui::Indent(14);
    ImGui::Dummy(ImVec2(0, 0));

    if (BeginSection("Behavior", 210)) {
        ImGui::Checkbox("Spawn automatically", &draft.spawnAutomatically);
        ImGui::Spacing();
        ImGui::BeginDisabled(!draft.spawnAutomatically);
        LabeledInputInt("Min delay (sec)", &draft.minSpawnDelaySec);
        ImGui::Spacing();
        LabeledInputInt("Max delay (sec)", &draft.maxSpawnDelaySec);
        ImGui::EndDisabled();
        ImGui::Spacing();
        LabeledInputInt("Duration (sec)", &draft.infectionDurationSec);
        draft.minSpawnDelaySec = std::clamp(draft.minSpawnDelaySec, 0, 86400);
        draft.maxSpawnDelaySec = std::clamp(draft.maxSpawnDelaySec, 0, 86400);
        draft.infectionDurationSec = std::clamp(draft.infectionDurationSec, 5, 86400);
    }
    EndSection();

    ImGui::Spacing();
    if (BeginSection("Death Consequences", 110)) {
        if (ImGui::Checkbox("Crash on death (resets off every launch)", &draft.crashOnDeath)) {
            if (draft.crashOnDeath) ImGui::OpenPopup("ConfirmCrash");
        }
        ImGui::Spacing();
        if (ImGui::Checkbox("Run command on death (resets off every launch)", &draft.execCmdOnDeath)) {
            if (draft.execCmdOnDeath) ImGui::OpenPopup("ConfirmExecCmd");
        }
        ImGui::Spacing();
        ImGui::BeginDisabled(!draft.execCmdOnDeath);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##cmd", cmdBuf, kCmdBufSize);
        ImGui::EndDisabled();
    }
    EndSection();

    ImGui::Spacing();
    if (BeginSection("Coins", 130)) {
        if (ImGui::RadioButton("Spawns in your own folders", !draft.useDrawerMode)) draft.useDrawerMode = false;
        if (ImGui::RadioButton("Spawns in \"drawers\"", draft.useDrawerMode)) draft.useDrawerMode = true;
        ImGui::Spacing();
        LabeledInputInt("Ransom amount", &draft.ransomAmount);
        draft.ransomAmount = std::clamp(draft.ransomAmount, 1, 1000000);
    }
    EndSection();

    ImGui::Unindent(14);
    ImGui::Spacing();
    ImGui::Spacing();

    float btnW = (winSize.x - 20 - 16) / 3.0f;
    ImGui::SetCursorPosX(10);
    if (ImGui::Button("Spawn Now", ImVec2(btnW, 30))) {
        result.closeRequested = true;
        result.accepted = true;
        result.spawnRequested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("OK", ImVec2(btnW, 30))) {
        result.closeRequested = true;
        result.accepted = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(btnW, 30))) {
        result.closeRequested = true;
    }

    ImGui::Spacing();
    const char* footer = "RANS0M C++ v2.0.5 by addavriance";
    float footerW = ImGui::CalcTextSize(footer).x;
    ImGui::SetCursorPosX((winSize.x - footerW) * 0.5f);
    ImGui::TextDisabled("%s", footer);

    ImGuiWindowFlags popupFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove;
    if (ImGui::BeginPopupModal("ConfirmCrash", nullptr, popupFlags)) {
        ImGui::PushTextWrapPos(320);
        ImGui::TextUnformatted(
            "This makes the timeout a REAL shutdown (or BSOD if run as admin) "
            "THIS RUN ONLY (resets off next launch).\n\nThis is not a simulation. Enable anyway?");
        ImGui::PopTextWrapPos();
        if (ImGui::Button("Yes", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(120, 0))) {
            draft.crashOnDeath = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopupModal("ConfirmExecCmd", nullptr, popupFlags)) {
        ImGui::PushTextWrapPos(320);
        ImGui::Text(
            "This will run the following command if the ransom times out "
            "unpaid THIS RUN ONLY (resets off next launch):\n\n%s\n\nEnable it?",
            cmdBuf);
        ImGui::PopTextWrapPos();
        if (ImGui::Button("Yes", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(120, 0))) {
            draft.execCmdOnDeath = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
    return result;
}

void CopyToCmdBuf(char* buf, const std::string& s) {
    std::snprintf(buf, kCmdBufSize, "%s", s.c_str());
}

} // namespace

struct ConfigWindow::Impl {
    std::unique_ptr<Window> window;
    ImGuiContext* imgui = nullptr;
    Config draft;
    char cmdBuf[kCmdBufSize] = {};
    ImTextureID starlightTex = nullptr;

    ~Impl() { Teardown(); }

    void Setup(const Config& initial, const ConfigWindowAssets& assets) {
        // ALWAYS_ON_TOP so it doesn't render behind other game windows on macOS (they're all on top too)
        window = std::make_unique<Window>("RANS0M Config", kWindowW, kWindowH, SDL_WINDOW_ALWAYS_ON_TOP,
                                           /*startVisible=*/true, /*titleBar=*/true);
        if (!window->Valid()) {
            window.reset();
            return;
        }

        draft = initial;
        CopyToCmdBuf(cmdBuf, initial.cmdOnDeath);

        imgui = ImGui::CreateContext();
        ImGui::SetCurrentContext(imgui);
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr; // no imgui.ini next to the exe
        if (!assets.fontPath.empty()) {
            const void* fontData = nullptr;
            size_t fontSize = 0;
            if (Platform::GetEmbeddedAssetBytes(assets.fontPath, &fontData, &fontSize)) {
                ImFontConfig cfg;
                cfg.FontDataOwnedByAtlas = false; // embedded data is owned by the exe's own image, not heap memory
                io.Fonts->AddFontFromMemoryTTF(const_cast<void*>(fontData), static_cast<int>(fontSize), 15.0f, &cfg);
            } else {
                io.Fonts->AddFontFromFileTTF(assets.fontPath.c_str(), 15.0f);
            }
        }
        ApplyRansomStyle();

        ImGui_ImplSDL2_InitForSDLRenderer(window->Raw(), window->Renderer());
        ImGui_ImplSDLRenderer2_Init(window->Renderer());

        if (!assets.starlightPath.empty()) {
            SDL_Texture* tex = window->LoadTexture(assets.starlightPath);
            starlightTex = reinterpret_cast<ImTextureID>(tex);
        }
    }

    void Teardown() {
        if (!window) return;
        ImGui::SetCurrentContext(imgui);
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext(imgui);
        imgui = nullptr;
        window.reset();
        starlightTex = nullptr;
    }
};

ConfigWindow::ConfigWindow() : impl_(std::make_unique<Impl>()) {}
ConfigWindow::~ConfigWindow() = default;

void ConfigWindow::Open(const Config& initial, const ConfigWindowAssets& assets) {
    if (impl_->window) {
        SDL_RaiseWindow(impl_->window->Raw());
        return;
    }
    impl_->Setup(initial, assets);
}

void ConfigWindow::Close() { impl_->Teardown(); }

void ConfigWindow::HandleEvent(const SDL_Event& e) {
    if (!impl_->window) return;
    Uint32 myId = impl_->window->Id();

    bool forThisWindow = false;
    switch (e.type) {
        case SDL_WINDOWEVENT:
            forThisWindow = (e.window.windowID == myId);
            if (forThisWindow && e.window.event == SDL_WINDOWEVENT_CLOSE) {
                impl_->Teardown();
                return;
            }
            break;
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
            forThisWindow = true; // SDL mouse events carry no windowID field
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        case SDL_TEXTINPUT:
        case SDL_TEXTEDITING:
            forThisWindow = (SDL_GetKeyboardFocus() == impl_->window->Raw());
            break;
        default:
            break;
    }
    if (!forThisWindow) return;

    ImGui::SetCurrentContext(impl_->imgui);
    ImGui_ImplSDL2_ProcessEvent(&e);
}

void ConfigWindow::Render() {
    if (!impl_->window) return;
    ImGui::SetCurrentContext(impl_->imgui);

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    FrameResult result = DrawConfigUI(impl_->draft, impl_->cmdBuf, impl_->starlightTex);

    ImGui::Render();
    SDL_Renderer* renderer = impl_->window->Renderer();
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    Platform::KeepWindowInAllSpaces(impl_->window->Raw()); // see Window::Present - this bypasses it
    SDL_RenderPresent(renderer);

    if (result.closeRequested) {
        bool accepted = result.accepted;
        bool spawnRequested = result.spawnRequested;
        Config cfg = impl_->draft;
        cfg.cmdOnDeath = impl_->cmdBuf;
        impl_->Teardown();
        if (accepted && onAccepted) onAccepted(cfg, spawnRequested);
    }
}

bool ConfigWindow::ShowModal(Config& cfg, const ConfigWindowAssets& assets, bool* spawnRequested) {
    if (spawnRequested) *spawnRequested = false;

    ConfigWindow modal;
    modal.impl_->Setup(cfg, assets);
    if (!modal.impl_->window) return false;

    bool accepted = false;
    while (modal.impl_->window) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                modal.impl_->Teardown();
                break;
            }
            modal.HandleEvent(e);
        }
        if (!modal.impl_->window) break;

        ImGui::SetCurrentContext(modal.impl_->imgui);
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        FrameResult result = DrawConfigUI(modal.impl_->draft, modal.impl_->cmdBuf, modal.impl_->starlightTex);

        ImGui::Render();
        SDL_Renderer* renderer = modal.impl_->window->Renderer();
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        Platform::KeepWindowInAllSpaces(modal.impl_->window->Raw());
        SDL_RenderPresent(renderer);

        if (result.closeRequested) {
            accepted = result.accepted;
            if (spawnRequested) *spawnRequested = result.spawnRequested;
            cfg = modal.impl_->draft;
            cfg.cmdOnDeath = modal.impl_->cmdBuf;
            modal.impl_->Teardown();
        }

        SDL_Delay(16);
    }
    return accepted;
}

} // namespace rd
