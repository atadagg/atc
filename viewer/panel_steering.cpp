#include <fstream>
#include <string>
#include <vector>

#include "app.hpp"
#include "imgui.h"
#include "model_steering.hpp"
#include "scenario.hpp"

namespace {

// A slider drags continuously, so committing every frame would append hundreds
// of interventions for one gesture. Commit once, when the control is released.
bool commit_on_release(const SteeringRow& row, double* value) {
    // Leave room for the label: the longest parameter name is ~26 characters,
    // and a clipped name is worse than a narrower slider.
    ImGui::SetNextItemWidth(-250.0f);
    ImGui::SliderScalar(row.name.data(), ImGuiDataType_Double, value, &row.lo, &row.hi,
                        "%.4f");
    return ImGui::IsItemDeactivatedAfterEdit();
}

void draw_save(App& app) {
    ImGui::InputText("path", app.scenario_path.data(), app.scenario_path.capacity() + 1,
                     ImGuiInputTextFlags_CallbackResize,
                     [](ImGuiInputTextCallbackData* d) {
                         auto* str = static_cast<std::string*>(d->UserData);
                         if (d->EventFlag == ImGuiInputTextFlags_CallbackResize) {
                             str->resize(static_cast<size_t>(d->BufTextLen));
                             d->Buf = str->data();
                         }
                         return 0;
                     },
                     &app.scenario_path);

    ImGui::SameLine();
    if (ImGui::Button("save scenario")) {
        Scenario s;
        s.seed = app.sim.seed();
        s.log = app.log;
        std::ofstream out(app.scenario_path);
        if (out) {
            scenario::write(s, out);
            app.scenario_status = "saved " + std::to_string(app.log.all().size()) +
                                  " intervention(s)";
        } else {
            app.scenario_status = "could not open that path";
        }
    }
    if (!app.scenario_status.empty()) {
        ImGui::TextDisabled("%s", app.scenario_status.c_str());
    }
}

}  // namespace

void draw_steering_panel(App& app) {
    // Wide enough for the longest parameter name beside its slider, and placed
    // clear of the other panels on a first run. FirstUseEver, so a layout the
    // user has arranged is never overridden.
    ImGui::SetNextWindowSize(ImVec2(660.0f, 720.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("steering");

    const int effective = app.sim.month();
    ImGui::TextWrapped(
        "Changes are scheduled, not applied: they take effect when month %d opens, "
        "before any agent acts. Until then the sliders keep showing what the "
        "simulation is actually using -- that is not a bug, it is the run being "
        "honest about what has happened so far.",
        effective);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Read fresh from the world every frame, so a seek that restores older
        // parameters is reflected without the panel keeping its own copy.
        for (const auto& row : steering_rows(app.sim.world())) {
            double value = row.value;
            if (commit_on_release(row, &value) && value != row.value) {
                app.log.set_param(effective, row.name, value);
            }
        }
    }

    if (ImGui::CollapsingHeader("issue money", ImGuiTreeNodeFlags_DefaultOpen)) {
        static double amount = 50.0;
        ImGui::InputDouble("per household", &amount, 10.0, 100.0, "%.2f");
        if (ImGui::Button("give every household")) {
            if (amount > 0.0) app.log.issue_to_all_households(effective, 0, amount);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("issuer 0 goes negative by the amount created");
    }

    if (ImGui::CollapsingHeader("what has been done", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto entries = app.log.all();
        if (entries.empty()) {
            ImGui::TextDisabled("nothing yet -- this run is just its seed");
        } else {
            ImGui::BeginChild("log", ImVec2(0, 140), true);
            for (const auto& e : entries) {
                const std::string line = describe(e);
                if (e.month <= app.sim.month()) {
                    ImGui::TextUnformatted(line.c_str());
                } else {
                    ImGui::TextDisabled("%s (pending)", line.c_str());
                }
            }
            ImGui::EndChild();
        }
        draw_save(app);
    }

    ImGui::End();
}
