#include "collection.hpp"
#include "core/imguid.hpp"
#include "settings.hpp"

#include <imgui.h>

namespace {

static std::unique_ptr<rtdxc::detail::directory_watcher> collection_watcher = {}; // prevents from closing process:::::
static std::filesystem::path last_collection_directory_path = {};

void watch_containers()
{
    if (last_collection_directory_path != global_settings.collection_directory_path) {
        collection_watcher.reset();
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

        for (const std::pair<std::filesystem::path, fmtdxc::project_info>& _container : global_containers) {
        }
    }
    ImGui::End();
}