#pragma once

#include <rtdxc/rtdxc.hpp>

inline unsigned int global_selected_daw_index {};
inline std::unique_ptr<rtdxc::session> global_session {};

void draw_controls();
