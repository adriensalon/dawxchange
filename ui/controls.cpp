#include "controls.hpp"
#include "collection.hpp"
#include "settings.hpp"

#include <imgui.h>

static std::filesystem::path _ableton_path = "C:/ProgramData/Ableton/Live 9 Standard/Program/Ableton Live 9 Standard.exe";
static const char* p2p_host_modal_id = "Create a P2P session##000";
static const char* p2p_client_modal_id = "Join a P2P session##001";

void draw_p2p_host_modal()
{
    const float width = 480.0f; // fixed modal width
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();

    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, FLT_MAX));

    if (ImGui::BeginPopupModal(p2p_host_modal_id, nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
        const char* text = "User projects will be stored as .dxcc in the collection directory. "
                           "This can be modified later from [Settings > Collection] or from the "
                           "settings.json next to the dawxchange executable.";

        float wrap_w = width - ImGui::GetStyle().WindowPadding.x * 2.0f;
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrap_w);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();

        ImGui::Spacing();
        ImGui::Spacing();

        

        ImGui::Spacing();

        // Save button full width
        bool ok = false;
        if (!ok)
            ImGui::BeginDisabled();
        if (ImGui::Button("Start", ImVec2(-FLT_MIN, 0.0f))) {
            // TODO
            
            ImGui::CloseCurrentPopup();
        }
        if (!ok)
            ImGui::EndDisabled();

        ImGui::EndPopup();
    }
}

void draw_controls()
{
    draw_p2p_host_modal();
    
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
                    global_session = std::make_unique<rtdxc::session>(rtdxc::local_session(
                        global_settings.daws_settings[global_selected_daw_index].version,
                        global_settings.daws_settings[global_selected_daw_index].executable_path,
                        global_selected_container_path.value(), []() {
                            return global_selected_container_path.value();
                        }));
                }
            }
            if (ImGui::Button("New P2P room")) {
                ImGui::OpenPopup(p2p_host_modal_id);
                // no first show modal to paste endpoint_data
                // global_session = std::make_unique<rtdxc::session>(rtdxc::p2p_client_session(
                //     global_settings.daws_settings[global_selected_daw_index].version,
                //     global_settings.daws_settings[global_selected_daw_index].executable_path,
                //     global_selected_container_path.value(), []() {
                //         return global_selected_container_path.value();
                //     },
                //     natp2p::endpoint_data {})); // TODO paste
            }
        }
        ImGui::End();
    }
}