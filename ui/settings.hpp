#pragma once

#include <rtdxc/rtdxc.hpp>

struct daw_settings {
    rtdxc::daw_version version;
    std::filesystem::path executable_path;
};

struct als_settings {
};

struct settings {
    std::filesystem::path collection_directory_path;
    std::vector<daw_settings> daws_settings;
    als_settings ableton_settings;
};

inline static bool global_show_settings {};
inline static unsigned int global_selected_settings_tab {};
inline static settings global_settings {};

void save_settings();
void load_settings();
void draw_settings();
