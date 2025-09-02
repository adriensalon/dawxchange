#include "controls.hpp"
#include "collection.hpp"
#include "settings.hpp"

#include <imgui.h>

static std::filesystem::path _ableton_path = "C:/ProgramData/Ableton/Live 9 Standard/Program/Ableton Live 9 Standard.exe";

void draw_controls()
{
    if (!global_session) {
        if (ImGui::Begin("Controls")) {

            // daw selection

            if (ImGui::Button("Import")) {
            }
            ImGui::SameLine();

            if (global_selected_container_path) {
                if (ImGui::Button("Export")) {
                    // open file dialog with filter for each daw
                    // that s it
                }
                ImGui::SameLine();
            }

            if (ImGui::Button("New")) {
            }
            ImGui::SameLine();

            if (global_selected_container_path) {
                if (ImGui::Button("Open")) {
                    global_session = std::make_unique<rtdxc::local_session>(
                        global_settings.daws_settings[global_selected_daw_index].version,
                        global_settings.daws_settings[global_selected_daw_index].executable_path,
                        global_selected_container_path.value(), []() {
                            return global_selected_container_path.value();
                        });
                }
            }
        }
        ImGui::End();
    }
}