#include "app.hpp"
#include "imgui.h"

void draw_agents_panel(App& app) {
    ImGui::Begin("agents");
    ImGui::Text("%zu firms, %zu households",
                app.sim.world().firms.size(), app.sim.world().households.size());
    ImGui::End();
}
