#pragma once

#include <rtdxc/rtdxc.hpp>

inline static std::vector<std::filesystem::path> global_container_paths {};
inline static std::optional<std::filesystem::path> global_selected_container_path {};

void draw_collection();