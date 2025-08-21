
cmake_minimum_required(VERSION 3.25.0)
project(rtdxc VERSION 3.7.1 DESCRIPTION "rtdxc")

option(RTALS_BUILD_UI "Build the ui executable" ON)

# rtdxc library
set(FMTALS_BUILD_DEMO OFF)
set(FMTALS_BUILD_TOOL OFF)
add_subdirectory(external/fmtals)
set(rtals_source
    source/client.cpp
    source/process.cpp
)
add_library(rtdxc STATIC ${rtals_source})
set_target_properties(rtdxc PROPERTIES CXX_STANDARD 17)
target_include_directories(rtdxc PUBLIC include)
target_link_libraries(rtdxc PRIVATE fmtals)

# ui executable
if(RTALS_BUILD_UI)
    add_subdirectory(external/imgui)
    add_executable(uidxc ui/main.cpp)
    set_target_properties(uidxc PROPERTIES CXX_STANDARD 17)
    target_link_libraries(uidxc PRIVATE rtdxc imgui)
endif()
