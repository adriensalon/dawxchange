#pragma once

#include <rtdxc/rtdxc.hpp>

inline static unsigned int global_selected_daw_index {};
inline static std::unique_ptr<rtdxc::session> global_session {};

void draw_controls();