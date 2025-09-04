#pragma once

#include <rtdxc/rtdxc.hpp>

inline std::optional<unsigned int> global_selected_container_index = {};
inline std::vector<std::pair<std::filesystem::path, fmtdxc::project_info>> global_containers = {};

void draw_collection();
