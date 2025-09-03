#include "collection.hpp"
#include "settings.hpp"

#include <imgui.h>

static std::unique_ptr<rtdxc::detail::directory_watcher> collection_watcher {}; // prevents from closing process:::::
static std::filesystem::path last_collection_directory_path {};

void draw_collection()
{
    if (last_collection_directory_path != global_settings.collection_directory_path) {
        // collection_watcher.reset();
        // collection_watcher = std::make_unique<rtdxc::detail::directory_watcher>(global_settings.collection_directory_path);
        last_collection_directory_path = global_settings.collection_directory_path;
    }
    
    if (ImGui::Begin("Collection##000000")) {







    }
    ImGui::End();
}