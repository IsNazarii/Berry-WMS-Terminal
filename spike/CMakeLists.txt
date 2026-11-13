# Spike (branch spike/history-ui only): the history screen on Qt Widgets next to QML.
find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(wms_history_widgets
    widgets/main.cpp
    widgets/theme.cpp
    widgets/empty_state.cpp
    widgets/delegates.cpp
    widgets/history_screen.cpp
)
target_link_libraries(wms_history_widgets PRIVATE Qt6::Widgets wms_ui)
target_compile_definitions(wms_history_widgets PRIVATE
    WMS_FONT_DIR="${CMAKE_SOURCE_DIR}/ui/style/fonts"
    WMS_ICON_DIR="${CMAKE_SOURCE_DIR}/ui/style/icons"
    WMS_SPIKE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/widgets")
target_compile_options(wms_history_widgets PRIVATE -Wall -Wextra -Wpedantic -Wshadow)

qt_add_executable(wms_history_qml qml/main.cpp)
target_link_libraries(wms_history_qml PRIVATE Qt6::Gui Qt6::Quick Qt6::QuickControls2
                      wms_ui wms_uiplugin wms_style wms_styleplugin)
target_compile_options(wms_history_qml PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
