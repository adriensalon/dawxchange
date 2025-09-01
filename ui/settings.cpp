#include "settings.hpp"
#include "core/dialog.hpp"

#include <cereal/archives/json.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/types/vector.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <fstream>

namespace std {
namespace filesystem {

    template <typename archive_t>
    void serialize(archive_t& archive, path& p)
    {
        if constexpr (archive_t::is_saving::value) {
            archive(cereal::make_nvp("path", p.string()));
        } else if constexpr (archive_t::is_loading::value) {
            std::string _str;
            archive(cereal::make_nvp("path", _str));
            p = std::filesystem::path(_str);
        }
    }

}
}

template <typename archive_t>
void serialize(archive_t& archive, daw_settings& value)
{
    archive(cereal::make_nvp("version", value.version));
    archive(cereal::make_nvp("executable_path", value.executable_path));
}

template <typename archive_t>
void serialize(archive_t& archive, als_settings& value)
{
}

template <typename archive_t>
void serialize(archive_t& archive, settings& value)
{
    archive(cereal::make_nvp("collection_directory_path", value.collection_directory_path));
    archive(cereal::make_nvp("daws_settings", value.daws_settings));
    archive(cereal::make_nvp("ableton_settings", value.ableton_settings));
}

void save_settings()
{
    std::filesystem::path _settings_path = std::filesystem::current_path() / "settings.json";
    std::ofstream _settings_stream(_settings_path, std::ios::binary);
    cereal::JSONOutputArchive _archive(_settings_stream);
    _archive(cereal::make_nvp("global_settings", global_settings));
}

void load_settings()
{
    std::filesystem::path _settings_path = std::filesystem::current_path() / "settings.json";
    if (std::filesystem::exists(_settings_path)) {
        std::ifstream _settings_stream(_settings_path, std::ios::binary);
        cereal::JSONInputArchive _archive(_settings_stream);
        _archive(cereal::make_nvp("global_settings", global_settings));
    } else {
        save_settings();
    }
}

static const char* select_collection_directory_modal_id = "Select collection directory##000";
static const char* select_daw_program_modal_id = "Select a first DAW program##001";

void draw_initial_collection_directory_modal()
{
    const float width = 400.0f; // your fixed width
    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();

    // lock width, let height auto-fit on first open
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, FLT_MAX));

    if (ImGui::BeginPopupModal(select_collection_directory_modal_id, nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
        // wrap to the content width (width - paddings)
        float wrap_w = width - ImGui::GetStyle().WindowPadding.x * 2.0f;
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrap_w);

        ImGui::TextUnformatted(
            "User projects will be stored as .dxcc in the collection directory. "
            "This can be modified later from [Settings > Collection] or from the "
            "settings.json next to the dawxchange executable.");
        ImGui::PopTextWrapPos();

        ImGui::Spacing();
        ImGui::Spacing();

        float full_w = ImGui::GetContentRegionAvail().x;
        float btn_w = 95.0f; // your fixed button width
        float spacing = ImGui::GetStyle().ItemSpacing.x; // horizontal spacing

        // Input = fill remaining space so that the button lands flush-right
        ImGui::SetNextItemWidth(full_w - btn_w - spacing);

        static std::string _collection_directory_str = "Path to the directory...";
        ImGui::InputText("##settings_input_text_001", &_collection_directory_str);
        ImGui::SameLine();
        if (ImGui::Button("Select...", ImVec2(btn_w, 0))) {
            if (auto p = pick_directory_dialog(std::filesystem::current_path()))
                _collection_directory_str = p->string();
        }

        ImGui::Spacing();
        bool ok = std::filesystem::is_directory(_collection_directory_str);
        if (!ok)
            ImGui::BeginDisabled();
        if (ImGui::Button("Save", ImVec2(-FLT_MIN, 0.0f))) {
            global_settings.collection_directory_path = _collection_directory_str;
            save_settings();
            ImGui::CloseCurrentPopup();
        }
        if (!ok)
            ImGui::EndDisabled();

        ImGui::EndPopup();
    }
}

void draw_initial_daw_program_modal()
{
    const float width = 480.0f; // fixed modal width
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, FLT_MAX));

    if (ImGui::BeginPopupModal(select_daw_program_modal_id, nullptr,
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

        // --- State ---
        enum class DawKind { AbletonALS,
            FLStudioFLP };
        static DawKind s_kind = DawKind::AbletonALS;
        static int s_version_idx = 0;
        static std::string _executable_path = "Path to the executable...";

        float full_w = ImGui::GetContentRegionAvail().x;

        // --- Row 1: DAW type dropdown (no visible label) ---
        ImGui::SetNextItemWidth(full_w);
        const char* kind_preview = (s_kind == DawKind::AbletonALS) ? "Ableton (.als)" : "FL Studio (.flp)";
        if (ImGui::BeginCombo("##daw_kind", kind_preview)) {
            if (ImGui::Selectable("Ableton (.als)", s_kind == DawKind::AbletonALS))
                s_kind = DawKind::AbletonALS, s_version_idx = 0;
            if (ImGui::Selectable("FL Studio (.flp)", s_kind == DawKind::FLStudioFLP))
                s_kind = DawKind::FLStudioFLP, s_version_idx = 0;
            ImGui::EndCombo();
        }

        ImGui::Spacing();

        // --- Row 2: Version dropdown (no visible label) ---
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        static const char* ALS_VERSIONS[] = { "v10", "v11", "v12" };
        static const char* FLP_VERSIONS[] = { "v20", "v21", "v22" };
        const char** versions = (s_kind == DawKind::AbletonALS) ? ALS_VERSIONS : FLP_VERSIONS;
        int count = (s_kind == DawKind::AbletonALS) ? IM_ARRAYSIZE(ALS_VERSIONS) : IM_ARRAYSIZE(FLP_VERSIONS);

        const char* version_preview = versions[std::clamp(s_version_idx, 0, count - 1)];
        if (ImGui::BeginCombo("##daw_version", version_preview)) {
            for (int i = 0; i < count; ++i) {
                bool selected = (i == s_version_idx);
                if (ImGui::Selectable(versions[i], selected))
                    s_version_idx = i;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();

        // --- Row 3: Input + button ---
        float btn_w = 95.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - btn_w - spacing);
        ImGui::InputText("##settings_input_text_002", &_executable_path);

        ImGui::SameLine();
        if (ImGui::Button("Select...", ImVec2(btn_w, 0))) {
            auto picked = open_file_dialog({ { "Executable", { ".exe" } } },
                std::filesystem::current_path());
            if (picked)
                _executable_path = picked->string();
        }

        ImGui::Spacing();

        // Save button full width
        bool ok = std::filesystem::exists(_executable_path);
        if (!ok)
            ImGui::BeginDisabled();
        if (ImGui::Button("Save", ImVec2(-FLT_MIN, 0.0f))) {
            // TODO
            global_settings.daws_settings.emplace_back(daw_settings { fmtals::version::v_9_7_7, _executable_path });
            save_settings();
            ImGui::CloseCurrentPopup();
        }
        if (!ok)
            ImGui::EndDisabled();

        ImGui::EndPopup();
    }
}

void draw_settings()
{
    if (!std::filesystem::is_directory(global_settings.collection_directory_path)) {
        ImGui::OpenPopup(select_collection_directory_modal_id);
    } else if (global_settings.daws_settings.empty()) {
        ImGui::OpenPopup(select_daw_program_modal_id);
    }
    draw_initial_collection_directory_modal();
    draw_initial_daw_program_modal();
    //

    if (global_settings.daws_settings.empty()) {
        // special modal
        // auto ok = open_file_dialog({
        //     { "Ableton Live", { ".als" } },
        //     { "FL Studio", { ".flp" } },
        // }, std::filesystem::current_path());
    }

    if (ImGui::Begin("Settings")) {
    }
    ImGui::End();
}

static std::filesystem::path _ableton_path = "C:/ProgramData/Ableton/Live 9 Standard/Program/Ableton Live 9 Standard.exe";