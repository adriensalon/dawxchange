#include "collection.hpp"
#include "core/imguid.hpp"
#include "settings.hpp"

#include <imgui.h>

#include <iomanip>
#include <sstream>

namespace {

static std::unique_ptr<rtdxc::detail::directory_watcher> collection_watcher = {}; // prevents from closing process:::::
static std::filesystem::path last_collection_directory_path = {};

enum struct collection_columns {
    name,
    created_on,
    modified_on
};

[[nodiscard]] static std::string format_time_point(const std::chrono::time_point<std::chrono::system_clock>& time_point)
{
    std::time_t _time = std::chrono::system_clock::to_time_t(time_point);
    std::tm* _tm = std::localtime(&_time); // okay for UI thread
    std::ostringstream _osstream;
    _osstream << std::put_time(_tm, "%Y-%m-%d %H:%M");
    return _osstream.str();
}

[[nodiscard]] static int compare_one(const std::pair<std::filesystem::path, fmtdxc::project_info>& a, const std::pair<std::filesystem::path, fmtdxc::project_info>& b, const ImGuiTableColumnSortSpecs& spec)
{
    switch (spec.ColumnIndex) {
    case collection_columns::name:
        if (a.first.filename().string() < b.first.filename().string())
            return -1;
        if (a.first.filename().string() > b.first.filename().string())
            return +1;
        return 0;
    case collection_columns::created_on:
        if (a.second.created_on < b.second.created_on)
            return -1;
        if (a.second.created_on > b.second.created_on)
            return +1;
        return 0;
    case collection_columns::modified_on:
        if (a.second.modified_on < b.second.modified_on)
            return -1;
        if (a.second.modified_on > b.second.modified_on)
            return +1;
        return 0;
    default:
        return 0;
    }
}

[[nodiscard]] static int index_comparator(const void* lhs, const void* rhs, void* user_data)
{
    const std::vector<std::pair<std::filesystem::path, fmtdxc::project_info>>& _items = *static_cast<const std::vector<std::pair<std::filesystem::path, fmtdxc::project_info>>*>(user_data);
    const int _a_index = *static_cast<const int*>(lhs);
    const int _b_index = *static_cast<const int*>(rhs);
    ImGuiTableSortSpecs* _sort_specs = ImGui::TableGetSortSpecs();

    for (int _index = 0; _index < _sort_specs->SpecsCount; _index++) {
        const ImGuiTableColumnSortSpecs& _specs = _sort_specs->Specs[_index];
        const int _delta = compare_one(_items[_a_index], _items[_b_index], _specs);
        if (_delta != 0) {
            return (_specs.SortDirection == ImGuiSortDirection_Ascending) ? _delta : -_delta;
        }
    }

    return (_a_index - _b_index);
}

void watch_containers()
{
    if (last_collection_directory_path != global_settings.collection_directory_path) {
        global_containers.clear();
        global_selected_container_index.reset();
        collection_watcher.reset();

        for (const std::filesystem::directory_entry& _container_entry : std::filesystem::recursive_directory_iterator(global_settings.collection_directory_path)) {
            std::pair<std::filesystem::path, fmtdxc::project_info>& _container = global_containers.emplace_back();
            _container.first = _container_entry.path();
            fmtdxc::scan_project(_container.first, _container.second);
        }

        collection_watcher = std::make_unique<rtdxc::detail::directory_watcher>(global_settings.collection_directory_path);

        collection_watcher->on_creation([](const std::filesystem::path& _created_container_path) {
            std::pair<std::filesystem::path, fmtdxc::project_info>& _created_container = global_containers.emplace_back();
            _created_container.first = _created_container_path;
            fmtdxc::scan_project(_created_container_path, _created_container.second);
        });

        collection_watcher->on_modification([](const std::filesystem::path& _modified_container_path) {
            unsigned int _index = 0;
            for (const std::pair<std::filesystem::path, fmtdxc::project_info>& _container : global_containers) {
                if (_container.first == _modified_container_path) {
                    std::pair<std::filesystem::path, fmtdxc::project_info>& _modified_container = global_containers[_index];
                    fmtdxc::scan_project(_modified_container_path, _modified_container.second);

                    break;
                }
                _index++;
            }
        });

        collection_watcher->on_removal([](const std::filesystem::path& _removed_container_path) {
            unsigned int _index = 0;
            for (const std::pair<std::filesystem::path, fmtdxc::project_info>& _container : global_containers) {
                if (_container.first == _removed_container_path) {
                    global_containers.erase(global_containers.begin() + _index);
                    break;
                }
                _index++;
            }
        });

        last_collection_directory_path = global_settings.collection_directory_path;
    }
}

}

void draw_collection()
{
    watch_containers();

    if (ImGui::Begin(IMGUID("Projects"))) {

        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_ScrollY;

        const float table_height = ImGui::GetTextLineHeightWithSpacing() * 12.0f;

        // Build/refresh index buffer
        static std::vector<int> order;
        static int last_size = -1;
        if (last_size != (int)global_containers.size()) {
            order.resize(global_containers.size());
            for (int i = 0; i < (int)global_containers.size(); ++i)
                order[i] = i;
            last_size = (int)global_containers.size();
        }

        if (ImGui::BeginTable(IMGUID("Projects"), 3, flags, ImVec2(-FLT_MIN, table_height))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn(IMGUID("Name"), ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_NoHide, 0.0f, (int)collection_columns::name);
            ImGui::TableSetupColumn(IMGUID("Created"), ImGuiTableColumnFlags_PreferSortDescending, 0.0f, (int)collection_columns::created_on);
            ImGui::TableSetupColumn(IMGUID("Modified"), ImGuiTableColumnFlags_PreferSortDescending, 0.0f, (int)collection_columns::modified_on);
            ImGui::TableHeadersRow();

            // Handle sorting
            if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
                if (sort_specs->SpecsDirty) {
                    std::sort(order.begin(), order.end(),
                        [&](int lhs, int rhs) {
                            for (int n = 0; n < sort_specs->SpecsCount; n++) {
                                const ImGuiTableColumnSortSpecs& spec = sort_specs->Specs[n];
                                int delta = compare_one(global_containers[lhs], global_containers[rhs], spec);
                                if (delta != 0) {
                                    return (spec.SortDirection == ImGuiSortDirection_Ascending) ? (delta < 0) : (delta > 0);
                                }
                            }
                            return lhs < rhs; // stable fallback
                        });

                    sort_specs->SpecsDirty = false;
                }
            }

            // Rows
            for (int display_n = 0; display_n < (int)order.size(); display_n++) {
                int i = order[display_n];
                const std::pair<std::filesystem::path, fmtdxc::project_info>& p = global_containers[i];

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex((int)collection_columns::name);

                bool selected = (global_selected_container_index && global_selected_container_index.value() == i);
                std::string row_label = p.first.filename().empty() ? std::string("<unnamed>##") + std::to_string(i)
                                                                   : p.first.filename().string() + "##" + std::to_string(i);

                if (ImGui::Selectable(row_label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    if (global_selected_container_index)
                        global_selected_container_index = i;
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    global_selected_container_index = order[display_n];
                }

                ImGui::TableSetColumnIndex((int)collection_columns::created_on);
                ImGui::TextUnformatted(format_time_point(p.second.created_on).c_str());

                ImGui::TableSetColumnIndex((int)collection_columns::modified_on);
                ImGui::TextUnformatted(format_time_point(p.second.modified_on).c_str());
            }

            ImGui::EndTable();
        }
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)
        && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)
        && !ImGui::IsAnyItemHovered()
        && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
        global_selected_container_index = std::nullopt;
    }

    ImGui::End();
}