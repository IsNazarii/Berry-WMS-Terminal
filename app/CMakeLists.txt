# Single entry point (course requirement): the Qt/QML terminal.
qt_add_executable(wms_terminal main.cpp)
# The *plugin targets register the BerryWms.Ui and BerryWms.Style QML modules in a static build.
target_link_libraries(wms_terminal PRIVATE Qt6::Gui Qt6::Quick Qt6::QuickControls2
                      wms_ui wms_uiplugin wms_style wms_styleplugin)
target_compile_options(wms_terminal PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
