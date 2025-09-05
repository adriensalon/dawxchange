#pragma once

#include <rtdxc/rtdxc.hpp>

struct project_info {
    std::chrono::time_point<std::chrono::system_clock> created_on;
    std::chrono::time_point<std::chrono::system_clock> modified_on;
    std::vector<fmtdxc::project_commit> commits;
    std::size_t applied;
};

inline std::optional<unsigned int> global_selected_container_index = {};
inline std::vector<std::pair<std::filesystem::path, project_info>> global_containers = {};

void draw_collection();
