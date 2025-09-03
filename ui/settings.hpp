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
    std::chrono::milliseconds p2p_natpmp_query_delay { 3000 };
    std::chrono::milliseconds p2p_upnp_query_delay { 3000 };
    als_settings ableton_settings;
};

// inline bool global_show_settings {};
// inline unsigned int global_selected_settings_tab {};
inline settings global_settings {};
inline bool global_initial_settings_defined = false;

void save_settings();
void load_settings();
void draw_settings();
void draw_initial_settings_modals();
