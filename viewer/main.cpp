#include <SDL3/SDL.h>

#include <cstdio>

#include "app.hpp"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace {

// Docks the four panels into a default arrangement the first time the viewer
// runs. ImGui's dockspace starts empty, so without this every panel opens as
// a free-floating window at its default position -- which is what you get if
// you delete imgui.ini and run.
//
// The split is chosen so the two cross-panel interactions stay visible at
// once: clicking a histogram bin filters the firm table, and dragging the
// scrub bar moves the distributions. Tabbing those apart would hide the
// result of the very actions that produce them.
//
// Only runs when the dockspace has no nodes, so a layout the user has
// rearranged (persisted in imgui.ini) is never overwritten.
// The four panels ImGui docks, and where we put them. Dock node sizes are
// absolute rather than ratios, so a layout built against the wrong viewport
// size collapses a column to a few pixels. SDL reports the size we asked for
// until the window manager has had its say -- measured here: 1600x1000 for two
// frames, then the real size -- so the layout is rebuilt whenever the viewport
// changes, for as long as the layout is still the one we made.
struct AutoLayout {
    ImGuiID run = 0;
    ImGuiID agents = 0;
    ImGuiID distributions = 0;
    ImGuiID time = 0;
    ImVec2 size{0.0f, 0.0f};
    bool ours = false;
};

// True while all four panels are still in the nodes we docked them into. Once
// the user moves one, the layout is theirs and we stop touching it -- including
// on window resize, where ImGui's own proportional rescaling takes over.
bool layout_is_unmodified(const AutoLayout& layout) {
    const auto docked_in = [](const char* name, ImGuiID node_id) {
        const ImGuiWindow* w = ImGui::FindWindowByName(name);
        return w != nullptr && w->DockId == node_id;
    };
    return docked_in("run", layout.run) && docked_in("agents", layout.agents) &&
           docked_in("distributions", layout.distributions) &&
           docked_in("time", layout.time);
}

// Docks the four panels into a default arrangement. ImGui's dockspace starts
// empty, so without this every panel opens as a free-floating window at its
// default position.
//
// The split keeps the two cross-panel interactions visible at once: clicking a
// histogram bin filters the firm table, and dragging the scrub bar moves the
// distributions. Tabbing those apart would hide the result of the very actions
// that produce them. Drag any panel's title bar onto another to tab them; that
// arrangement persists in imgui.ini and is never overwritten here.
void build_default_layout(ImGuiID dockspace_id) {
    static AutoLayout layout;

    const ImVec2 size = ImGui::GetMainViewport()->WorkSize;
    if (size.x <= 0.0f || size.y <= 0.0f) return;

    const ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
    if (node != nullptr && node->IsSplitNode()) {
        // A layout already exists: either restored from imgui.ini, or ours.
        if (!layout.ours) return;
        if (!layout_is_unmodified(layout)) {
            layout.ours = false;
            return;
        }
        if (size.x == layout.size.x && size.y == layout.size.y) return;
    }

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, size);

    // A short strip across the top for the run controls, then the remainder
    // split left (agents) / right, with the right column stacked
    // distributions over time.
    ImGuiID body = 0;
    const ImGuiID top =
        ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Up, 0.11f, nullptr, &body);
    ImGuiID right = 0;
    const ImGuiID left = ImGui::DockBuilderSplitNode(body, ImGuiDir_Left, 0.55f, nullptr, &right);
    ImGuiID right_bottom = 0;
    const ImGuiID right_top =
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.55f, nullptr, &right_bottom);

    ImGui::DockBuilderDockWindow("run", top);
    ImGui::DockBuilderDockWindow("agents", left);
    ImGui::DockBuilderDockWindow("distributions", right_top);
    ImGui::DockBuilderDockWindow("time", right_bottom);
    ImGui::DockBuilderFinish(dockspace_id);

    layout = AutoLayout{top, left, right_top, right_bottom, size, true};
}

}  // namespace

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("economy", 1600, 1000, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (window == nullptr || renderer == nullptr) {
        std::fprintf(stderr, "SDL window/renderer failed: %s\n", SDL_GetError());
        return 1;
    }

    // Vsync caps the render loop at the display refresh rate. Without it the
    // loop spins as fast as the GPU allows, and `months_per_frame` paces the
    // simulation against that unbounded frame rate rather than ~60 Hz. Not
    // fatal if unavailable (e.g. headless/software rendering) — the viewer
    // just free-runs in that case, so don't abort startup over it.
    if (!SDL_SetRenderVSync(renderer, 1)) {
        std::fprintf(stderr, "SDL_SetRenderVSync failed: %s\n", SDL_GetError());
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    App app(1);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT) running = false;
        }

        app.advance();

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        const ImGuiID dockspace_id = ImGui::DockSpaceOverViewport();
        build_default_layout(dockspace_id);

        ImGui::Begin("run");
        ImGui::Text("month %d", app.sim.month());
        if (ImGui::Button(app.paused ? "run" : "pause")) app.paused = !app.paused;
        ImGui::SameLine();
        if (ImGui::Button("step")) {
            const bool was_paused = app.paused;
            app.paused = false;
            app.advance();
            app.paused = was_paused;
        }
        ImGui::SliderInt("months/frame", &app.months_per_frame, 1, 60);
        ImGui::End();

        draw_agents_panel(app);
        draw_dist_panel(app);
        draw_time_panel(app);

        ImGui::Render();
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
