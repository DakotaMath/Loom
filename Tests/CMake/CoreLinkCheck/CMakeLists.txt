# A test project for loom_check_no_ui_links (Source/CMake/LoomCoreLinkCheck.cmake).
# Configured by Tests/CMakeLists.txt once per case, with LINK_CHECK_CASE set; stand-in INTERFACE
# targets play Qt's modules, so no compiler or Qt is needed. Each case except `clean` must stop
# the configure with the check's message.
cmake_minimum_required(VERSION 3.28)
project(CoreLinkCheck LANGUAGES NONE)

include("${LOOM_CMAKE_DIR}/LoomCoreLinkCheck.cmake")

foreach(module Core Gui Widgets WebEngineWidgets)
    add_library(Qt6::${module} INTERFACE IMPORTED)
endforeach()
set_property(TARGET Qt6::Widgets PROPERTY INTERFACE_LINK_LIBRARIES Qt6::Gui)
add_library(Qt::Widgets ALIAS Qt6::Widgets)
add_library(tomlplusplus INTERFACE)

add_library(Helper INTERFACE)
add_library(LoomCore INTERFACE)
target_link_libraries(LoomCore INTERFACE Qt6::Core tomlplusplus)

if(LINK_CHECK_CASE STREQUAL "clean")
    target_link_libraries(LoomCore INTERFACE Helper)
elseif(LINK_CHECK_CASE STREQUAL "direct-gui")
    target_link_libraries(LoomCore INTERFACE Qt6::Gui)
elseif(LINK_CHECK_CASE STREQUAL "direct-widgets")
    target_link_libraries(LoomCore INTERFACE Qt6::Widgets)
elseif(LINK_CHECK_CASE STREQUAL "webengine")
    target_link_libraries(LoomCore INTERFACE Qt6::WebEngineWidgets)
elseif(LINK_CHECK_CASE STREQUAL "through-target")
    target_link_libraries(Helper INTERFACE Qt6::Gui)
    target_link_libraries(LoomCore INTERFACE Helper)
elseif(LINK_CHECK_CASE STREQUAL "link-only")
    # How a static library's PRIVATE link shows up in its interface.
    target_link_libraries(Helper INTERFACE Qt6::Widgets)
    target_link_libraries(LoomCore INTERFACE "$<LINK_ONLY:Helper>")
elseif(LINK_CHECK_CASE STREQUAL "versionless")
    target_link_libraries(LoomCore INTERFACE Qt::Widgets)
else()
    message(FATAL_ERROR "Unknown LINK_CHECK_CASE: '${LINK_CHECK_CASE}'")
endif()

loom_check_no_ui_links(LoomCore)
message(STATUS "core-link-check: passed")
