cmake_minimum_required(VERSION 4.4.2)
project(reaper_ioj VERSION 0.1.0 LANGUAGES CXX RC)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND MSVC_VERSION GREATER_EQUAL 1944)
    # CMake currently maps C++23 to /std:c++latest, which also enables later features.
    set(CMAKE_CXX23_STANDARD_COMPILE_OPTION "-std:c++23preview")
    set(CMAKE_CXX23_EXTENSION_COMPILE_OPTION "-std:c++23preview")
endif()

set(REAPER_SDK_ROOT "C:/dev/reaper-sdk" CACHE PATH "REAPER extension SDK checkout")
find_path(REAPER_SDK_INCLUDE_DIR reaper_plugin_functions.h
    PATHS "${REAPER_SDK_ROOT}/sdk" NO_DEFAULT_PATH REQUIRED NO_CACHE)

add_library(ioj_regions STATIC)
target_sources(ioj_regions
    PRIVATE lib/src/rename_regions.cpp
    PUBLIC FILE_SET HEADERS
        BASE_DIRS lib/include
        FILES lib/include/reaper_ioj/rename_regions.hpp
)
target_compile_features(ioj_regions PUBLIC cxx_std_23)

configure_file(plugin/version.rc.in version.rc @ONLY)
add_library(reaper_ioj MODULE)
target_sources(reaper_ioj PRIVATE
    plugin/extension.cpp
    plugin/rename_regions_action.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
)
target_include_directories(reaper_ioj SYSTEM PRIVATE "${REAPER_SDK_INCLUDE_DIR}")
target_compile_definitions(reaper_ioj PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(reaper_ioj PRIVATE ioj_regions user32)
install(TARGETS reaper_ioj LIBRARY DESTINATION UserPlugins RUNTIME DESTINATION UserPlugins)

include(CTest)
if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    add_executable(ioj_region_tests)
    target_sources(ioj_region_tests PRIVATE tests/rename_regions_test.cpp)
    target_link_libraries(ioj_region_tests PRIVATE ioj_regions GTest::gtest_main)
    add_custom_command(TARGET ioj_region_tests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy -t "$<TARGET_FILE_DIR:ioj_region_tests>"
            $<TARGET_RUNTIME_DLLS:ioj_region_tests>
        COMMAND_EXPAND_LISTS VERBATIM)
    include(GoogleTest)
    gtest_discover_tests(ioj_region_tests)
endif()
