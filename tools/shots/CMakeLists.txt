# wms_shots: screenshots of every screen without a window, for before/after checks of UI changes.
# Needs Qt Test (QTest::keyClick goes through the window's shortcuts like a real key press).
find_package(Qt6 QUIET COMPONENTS Test)
if(NOT Qt6Test_FOUND)
    message(STATUS "Qt Test not found: wms_shots is not built")
    return()
endif()

qt_add_executable(wms_shots main.cpp)
target_link_libraries(wms_shots PRIVATE Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Test
                      wms_ui wms_uiplugin wms_style wms_styleplugin)
target_compile_options(wms_shots PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
