#include "controls.hpp"
#include "collection.hpp"
#include "core/base64.hpp"
#include "settings.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <future>

static std::filesystem::path _ableton_path = "C:/ProgramData/Ableton/Live 9 Standard/Program/Ableton Live 9 Standard.exe";
static const char* p2p_host_modal_id = "Create a P2P session##0022";
static const char* p2p_client_modal_id = "Join a P2P session##0032";

static std::future<std::vector<natp2p::endpoint_lease>> _host_endpoints_future;
static std::vector<natp2p::endpoint_lease> _host_endpoints;
static bool _have_endpoints_arrived;
static unsigned int _selected_endpoint;

std::string format_type(const natp2p::endpoint_type type)
{
    if (type == natp2p::endpoint_type::ipv6_global) {
        return "GLOBAL IPv6";
    } else if (type == natp2p::endpoint_type::ipv4_mapped_natpmp) {
        return "NATPMP IPv4";
    } else if (type == natp2p::endpoint_type::ipv4_mapped_upnp) {
        return "UPNP IPv4";
    } else if (type == natp2p::endpoint_type::ipv4_lan) {
        return "LAN IPv4";
    }
    throw std::runtime_error("");
}

std::string format_endpoint(const natp2p::endpoint_lease& endpoint)
{
    return format_type(endpoint.data.type) + " - " + endpoint.data.external_ip + ":" + std::to_string(endpoint.data.external_port);
}

void draw_p2p_host_modal()
{
    const float width = 400.0f;
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();

    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, FLT_MAX));

    if (ImGui::BeginPopupModal(p2p_host_modal_id, nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
        if (!_have_endpoints_arrived && _host_endpoints_future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
            const char* text = "Acquiring available endpoints for hosting a P2P session...";
            float wrap_w = width - ImGui::GetStyle().WindowPadding.x * 2.0f;
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrap_w);
            ImGui::TextUnformatted(text);
            ImGui::PopTextWrapPos();

        } else {
            if (!_have_endpoints_arrived) {
                _host_endpoints = _host_endpoints_future.get();
                _have_endpoints_arrived = true;

            } else {
                std::string _preview = format_endpoint(_host_endpoints[_selected_endpoint]).c_str();
                natp2p::endpoint_type _selected_type = _host_endpoints[_selected_endpoint].data.type;
                std::string _type_info;
                if (_selected_type == natp2p::endpoint_type::ipv6_global) {
                    _type_info = "";
                } else if (_selected_type == natp2p::endpoint_type::ipv4_mapped_natpmp) {
                    _type_info = "";
                } else if (_selected_type == natp2p::endpoint_type::ipv4_mapped_upnp) {
                    _type_info = "";
                } else if (_selected_type == natp2p::endpoint_type::ipv4_lan) {
                    _type_info = "LAN endpoints are available on most configurations but can only be reached "
                                 "from the same local network";
                }

                float wrap_w = width - ImGui::GetStyle().WindowPadding.x * 2.0f;
                float btn_w = 95.0f;
                float spacing = ImGui::GetStyle().ItemSpacing.x;

                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrap_w);
                ImGui::TextUnformatted(_type_info.c_str());
                ImGui::PopTextWrapPos();
                ImGui::Spacing();
                ImGui::Spacing();

                float full_w = ImGui::GetContentRegionAvail().x;
                ImGui::SetNextItemWidth(full_w);
                if (ImGui::BeginCombo("##endpoint_combo_0", _preview.c_str())) {
                    unsigned int _index = 0;
                    for (const natp2p::endpoint_lease& _endpoint : _host_endpoints) {
                        std::string _selectable_preview = format_endpoint(_host_endpoints[_index]).c_str();
                        if (ImGui::Selectable(_selectable_preview.c_str())) {
                            _selected_endpoint = _index;
                        }
                        _index++;
                    }
                    ImGui::EndCombo();
                }
                ImGui::Spacing();

                std::string _encoded_endpoint = encode_base64(natp2p::encode_endpoint(_host_endpoints[_selected_endpoint].data));
                ImGui::SetNextItemWidth(full_w - btn_w - spacing);
                ImGui::BeginDisabled();
                ImGui::InputText("##encoded_endpoint_xx", &_encoded_endpoint);
                ImGui::EndDisabled();
                ImGui::SameLine();

                if (ImGui::Button("Copy", ImVec2(btn_w, 0))) {
                    // copy to clipboard
                }
                ImGui::Spacing();

                if (ImGui::Button("Start", ImVec2(-FLT_MIN, 0.0f))) {
                    // TODO

                    ImGui::CloseCurrentPopup();
                }
            }
        }

        ImGui::EndPopup();
    }
}

void draw_p2p_client_modal()
{
}

void draw_controls()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    float _ribbon_width = ImGui::GetMainViewport()->Size.x;
    float _ribbon_height = ImGui::GetStyle().WindowPadding.y * 2 + ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(_ribbon_width, _ribbon_height));
    if (!global_session) {
        if (ImGui::Begin("Controls", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus)) {

            if (ImGui::Button("Import")) {
            }
            ImGui::SameLine();

            if (!global_selected_container_path) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Export")) {
                // open file dialog with filter for each daw
                // that s it
            }
            if (!global_selected_container_path) {
                ImGui::EndDisabled();
            }
            ImGui::SameLine();

            float _daw_selection_combo_width = 250.f;
            std::string _selected_daw_preview = global_settings.daws_settings[global_selected_daw_index].executable_path.filename().replace_extension("").string();
            ImGui::SetNextItemWidth(_daw_selection_combo_width);
            if (ImGui::BeginCombo("##daw_selected", _selected_daw_preview.c_str())) {
                unsigned int _index = 0;
                for (const daw_settings& _daw_settings : global_settings.daws_settings) {
                    std::string _daw_preview = global_settings.daws_settings[global_selected_daw_index].executable_path.filename().replace_extension("").string();
                    if (ImGui::Selectable(_daw_preview.c_str())) {
                        global_selected_daw_index = _index;
                    }
                    _index++;
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();

            if (ImGui::Button("New")) {
                global_session = std::make_unique<rtdxc::session>(rtdxc::local_session(
                    global_settings.daws_settings[global_selected_daw_index].version,
                    global_settings.daws_settings[global_selected_daw_index].executable_path,
                    std::nullopt, []() {
                        return ""; // TODO MODAL
                    }));
            }
            ImGui::SameLine();

            if (!global_selected_container_path) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("New (from template)")) {
            }
            if (!global_selected_container_path) {
                ImGui::EndDisabled();
            }
            ImGui::SameLine();

            if (ImGui::Button("New (as P2P host)")) {
                _host_endpoints_future = std::async(natp2p::acquire_endpoints, 444, natp2p::transport_protocol::udp);
                _host_endpoints.clear();
                _have_endpoints_arrived = false;
                _selected_endpoint = 0;
                ImGui::OpenPopup(p2p_host_modal_id);
            }
            ImGui::SameLine();

            if (!global_selected_container_path) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Open")) {
                global_session = std::make_unique<rtdxc::session>(rtdxc::local_session(
                    global_settings.daws_settings[global_selected_daw_index].version,
                    global_settings.daws_settings[global_selected_daw_index].executable_path,
                    global_selected_container_path.value(), []() {
                        return global_selected_container_path.value();
                    }));
            }
            if (!global_selected_container_path) {
                ImGui::EndDisabled();
            }
            ImGui::SameLine();

            if (!global_selected_container_path) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Open (as P2P host)")) {
            }
            if (!global_selected_container_path) {
                ImGui::EndDisabled();
            }
            ImGui::SameLine();

            if (ImGui::Button("Join (as P2P client)")) {
                ImGui::OpenPopup(p2p_client_modal_id);
            }
            ImGui::SameLine();

            ImGuiStyle& _style = ImGui::GetStyle();
            float _settings_button_size = ImGui::CalcTextSize("Settings").x + _style.FramePadding.x * 2;
            float _content_region_available = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + _content_region_available - _settings_button_size);

            if (ImGui::Button("Settings")) {
            }
        }
        draw_p2p_host_modal();
        ImGui::End();
    }

    float _dockspace_height = ImGui::GetMainViewport()->Size.y - _ribbon_height;
    ImGui::SetNextWindowPos(ImVec2(0, _ribbon_height));
    ImGui::SetNextWindowSize(ImVec2(_ribbon_width, _dockspace_height));
    if (ImGui::Begin("##dockspace))", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
        ImGui::End();
    }
    ImGui::PopStyleVar(2);
}