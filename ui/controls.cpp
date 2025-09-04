#include "controls.hpp"
#include "collection.hpp"
#include "core/base64.hpp"
#include "core/imguid.hpp"
#include "settings.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <future>

namespace {

static const char* p2p_host_modal_id = IMGUID("Create a P2P session");
static std::future<std::vector<natp2p::endpoint_lease>> p2p_host_endpoints_future;
static std::vector<natp2p::endpoint_lease> p2p_host_endpoints;
static unsigned int p2p_host_selected_endpoint;
static bool is_p2p_endpoints_arrived;

static const char* p2p_client_modal_id = IMGUID("Join a P2P session");

static const char* daw_loading_modal_id = IMGUID("Opening DAW");
static std::future<void> daw_loading_future = {};

[[nodiscard]] std::string format_type(const natp2p::endpoint_type type)
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
    throw std::invalid_argument("Invalid endpoint type");
}

[[nodiscard]] std::string format_endpoint(const natp2p::endpoint_lease& endpoint)
{
    return format_type(endpoint.data.type)
        + " - " + endpoint.data.external_ip
        + ":" + std::to_string(endpoint.data.external_port);
}

void draw_import_control()
{
    if (ImGui::Button(IMGUID("Import"))) {
    }
    ImGui::SameLine();
}

void draw_export_control()
{
    if (!global_selected_container_index) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(IMGUID("Export"))) {
        // open file dialog with filter for each daw
        // that s it
    }
    if (!global_selected_container_index) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
}

void draw_daw_selection_control()
{
    const float _daw_selection_combo_width = 250.f;
    const std::string _selected_daw_preview = global_settings.daws_settings[global_selected_daw_index].executable_path.filename().replace_extension("").string();
    ImGui::SetNextItemWidth(_daw_selection_combo_width);

    if (ImGui::BeginCombo(IMGUIDU, _selected_daw_preview.c_str())) {
        unsigned int _index = 0;
        for (const daw_settings& _daw_settings : global_settings.daws_settings) {
            const std::string _daw_preview = global_settings.daws_settings[global_selected_daw_index].executable_path.filename().replace_extension("").string();
            if (ImGui::Selectable(_daw_preview.c_str())) {
                global_selected_daw_index = _index;
            }
            _index++;
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
}

void draw_new_control()
{
    if (ImGui::Button(IMGUID("New"))) {
        daw_loading_future = std::async([]() {
            global_session = std::make_unique<rtdxc::session>(rtdxc::local_session(
                global_settings.daws_settings[global_selected_daw_index].version,
                global_settings.daws_settings[global_selected_daw_index].executable_path,
                std::nullopt, []() {
                    return ""; // TODO MODAL
                }));
        });
        ImGui::OpenPopup(daw_loading_modal_id);
    }
    ImGui::SameLine();
}

void draw_new_as_p2p_host_control()
{
    if (ImGui::Button(IMGUID("New (as P2P host)"))) {
        p2p_host_endpoints_future = std::async(natp2p::acquire_endpoints, 444, natp2p::transport_protocol::udp);
        p2p_host_endpoints.clear();
        is_p2p_endpoints_arrived = false;
        p2p_host_selected_endpoint = 0;
        ImGui::OpenPopup(p2p_host_modal_id);
    }
    ImGui::SameLine();
}

void draw_new_from_template_control()
{
    if (!global_selected_container_index) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(IMGUID("New (from template)"))) {
    }
    if (!global_selected_container_index) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
}

void draw_open_control()
{
    if (!global_selected_container_index) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(IMGUID("Open"))) {
        const std::filesystem::path _selected_container_path = global_containers[global_selected_container_index.value()].first;
        global_session = std::make_unique<rtdxc::session>(rtdxc::local_session(
            global_settings.daws_settings[global_selected_daw_index].version,
            global_settings.daws_settings[global_selected_daw_index].executable_path,
            _selected_container_path, [_selected_container_path]() {
                return _selected_container_path;
            }));
    }
    if (!global_selected_container_index) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
}

void draw_open_as_p2p_host_control()
{
    if (!global_selected_container_index) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(IMGUID("Open (as P2P host)"))) {
    }
    if (!global_selected_container_index) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
}

void draw_join_as_p2p_client_control()
{
    if (ImGui::Button(IMGUID("Join (as P2P client)"))) {
        ImGui::OpenPopup(p2p_client_modal_id);
    }
    ImGui::SameLine();
}

void draw_settings_control()
{
    ImGuiStyle& _style = ImGui::GetStyle();
    const float _settings_button_size = ImGui::CalcTextSize("Settings").x + _style.FramePadding.x * 2;
    const float _content_region_available = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + _content_region_available - _settings_button_size);

    if (ImGui::Button(IMGUID("Settings"))) {
    }
}

void draw_p2p_host_modal()
{
    const float _modal_width = 400.f;
    const ImVec2 _viewport_center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(_viewport_center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(_modal_width, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(_modal_width, 0.f), ImVec2(_modal_width, FLT_MAX));

    if (ImGui::BeginPopupModal(p2p_host_modal_id, nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {

        if (!is_p2p_endpoints_arrived && p2p_host_endpoints_future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
            const char* text = "Acquiring available endpoints for hosting a P2P session...";
            const float _wrap_width = _modal_width - ImGui::GetStyle().WindowPadding.x * 2.f;
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + _wrap_width);
            ImGui::TextUnformatted(text);
            ImGui::PopTextWrapPos();

        } else {
            if (!is_p2p_endpoints_arrived) {
                p2p_host_endpoints = p2p_host_endpoints_future.get();
                is_p2p_endpoints_arrived = true;

            } else {
                const std::string _preview = format_endpoint(p2p_host_endpoints[p2p_host_selected_endpoint]).c_str();
                const natp2p::endpoint_type _selected_type = p2p_host_endpoints[p2p_host_selected_endpoint].data.type;
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

                const float _wrap_width = _modal_width - ImGui::GetStyle().WindowPadding.x * 2.f;
                const float _button_width = 95.f;
                const float _spacing_width = ImGui::GetStyle().ItemSpacing.x;
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + _wrap_width);
                ImGui::TextUnformatted(_type_info.c_str());
                ImGui::PopTextWrapPos();
                ImGui::Spacing();
                ImGui::Spacing();

                float full_w = ImGui::GetContentRegionAvail().x;
                ImGui::SetNextItemWidth(full_w);
                if (ImGui::BeginCombo(IMGUIDU, _preview.c_str())) {
                    unsigned int _index = 0;
                    for (const natp2p::endpoint_lease& _endpoint : p2p_host_endpoints) {
                        std::string _selectable_preview = format_endpoint(p2p_host_endpoints[_index]).c_str();
                        if (ImGui::Selectable(_selectable_preview.c_str())) {
                            p2p_host_selected_endpoint = _index;
                        }
                        _index++;
                    }
                    ImGui::EndCombo();
                }
                ImGui::Spacing();

                std::string _encoded_endpoint = encode_base64(natp2p::encode_endpoint(p2p_host_endpoints[p2p_host_selected_endpoint].data));
                ImGui::SetNextItemWidth(full_w - _button_width - _spacing_width);
                ImGui::BeginDisabled();
                ImGui::InputText(IMGUIDU, &_encoded_endpoint);
                ImGui::EndDisabled();
                ImGui::SameLine();

                if (ImGui::Button(IMGUID("Copy"), ImVec2(_button_width, 0))) {
                    // copy to clipboard
                }
                ImGui::Spacing();

                if (ImGui::Button(IMGUID("Start"), ImVec2(-FLT_MIN, 0.f))) {
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

void draw_daw_loading_modal()
{
    const float _modal_width = 400.f;
    const ImVec2 _viewport_center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(_viewport_center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(_modal_width, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(_modal_width, 0.f), ImVec2(_modal_width, FLT_MAX));

    if (ImGui::BeginPopupModal(daw_loading_modal_id, nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {

        const std::string _text = global_settings.daws_settings[global_selected_daw_index].executable_path.filename().replace_extension("").string().append(" is starting...");
        const float _wrap_width = _modal_width - ImGui::GetStyle().WindowPadding.x * 2.f;
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + _wrap_width);
        ImGui::TextUnformatted(_text.c_str());
        ImGui::PopTextWrapPos();

        if (daw_loading_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            daw_loading_future.get();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

}

void draw_controls()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));

    const float _ribbon_width = ImGui::GetMainViewport()->Size.x;
    const float _ribbon_height = ImGui::GetStyle().WindowPadding.y * 2
        + ImGui::GetFontSize()
        + ImGui::GetStyle().FramePadding.y * 2;

    if (!global_session) {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(_ribbon_width, _ribbon_height));

        const ImGuiWindowFlags _controls_window_flags = ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoBringToFrontOnFocus;

        if (ImGui::Begin(IMGUID("Controls"), 0, _controls_window_flags)) {
            draw_import_control();
            draw_export_control();
            draw_daw_selection_control();
            draw_new_control();
            draw_new_from_template_control();
            draw_new_as_p2p_host_control();
            draw_open_control();
            draw_open_as_p2p_host_control();
            draw_join_as_p2p_client_control();
            draw_settings_control();
        }

        draw_p2p_host_modal();
        draw_daw_loading_modal();
        ImGui::End();
    }

    const float _dockspace_height = ImGui::GetMainViewport()->Size.y - _ribbon_height;
    ImGui::SetNextWindowPos(ImVec2(0, _ribbon_height));
    ImGui::SetNextWindowSize(ImVec2(_ribbon_width, _dockspace_height));

    const ImGuiWindowFlags _dockspace_window_flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin(IMGUIDU, 0, _dockspace_window_flags)) {
        ImGui::End();
    }

    ImGui::PopStyleVar(2);
}
