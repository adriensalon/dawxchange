
cmake_minimum_required(VERSION 3.25.0)
project(rtdxc VERSION 3.7.1 DESCRIPTION "rtdxc")

option(RTDXC_BUILD_TOOL "Build tool executables" ON)
option(RTDXC_BUILD_UI "Build the ui executable" ON)

# rtdxc library
set(CEREAL_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/external/cereal/include)
add_subdirectory(external/fmtdxc)
add_subdirectory(external/fmtals)
add_subdirectory(external/natp2p)
add_subdirectory(external/enet)
if(WIN32)
    target_compile_definitions(enet PRIVATE _WINSOCK_DEPRECATED_NO_WARNINGS)
endif()
file(GLOB_RECURSE rtdxc_source "source/*.cpp")
add_library(rtdxc STATIC ${rtdxc_source})
set_target_properties(rtdxc PROPERTIES CXX_STANDARD 17)
target_include_directories(rtdxc PUBLIC include)
target_include_directories(rtdxc PRIVATE ${CEREAL_INCLUDE_DIR})
target_include_directories(rtdxc PRIVATE external/enet/include)
target_link_libraries(rtdxc PUBLIC fmtals fmtdxc natp2p enet)

# tools
if(RTDXC_BUILD_TOOL)
    add_executable(als2dxcc "tool/als2dxcc.cpp")
    set_target_properties(als2dxcc PROPERTIES CXX_STANDARD 17)
    target_link_libraries(als2dxcc PRIVATE rtdxc)
    add_executable(dxcc2als "tool/dxcc2als.cpp")
    set_target_properties(dxcc2als PROPERTIES CXX_STANDARD 17)
    target_link_libraries(dxcc2als PRIVATE rtdxc)
endif()

# ui
if(RTDXC_BUILD_UI)
    add_subdirectory(external/imgui)
    file(GLOB_RECURSE dawxchange_source "ui/*.cpp")
    add_executable(dawxchange ${dawxchange_source})
    target_include_directories(dawxchange PRIVATE ${CEREAL_INCLUDE_DIR})
    set_target_properties(dawxchange PROPERTIES CXX_STANDARD 17)
    target_link_libraries(dawxchange PRIVATE rtdxc imgui)
endif()
