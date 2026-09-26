# Single entry point (course requirement): the Qt/QML terminal.
qt_add_executable(wms_terminal main.cpp)
# wms_uiplugin registers the BerryWms.Ui QML module in a static build.
target_link_libraries(wms_terminal PRIVATE Qt6::Gui Qt6::Quick wms_ui wms_uiplugin)
target_compile_options(wms_terminal PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
