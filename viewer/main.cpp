#include <SDL3/SDL.h>

#include <cstdio>

#include "app.hpp"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "imgui.h"

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
        ImGui::DockSpaceOverViewport();

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
