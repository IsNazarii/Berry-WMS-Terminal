# The terminal's own Qt Quick Controls style: design tokens (Theme) plus the
# controls drawn from them. Controls not listed here fall back to Basic.
set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_library(wms_style STATIC)
qt_add_qml_module(wms_style
    URI BerryWms.Style
    IMPORTS QtQuick.Controls.Basic
    QML_FILES
        Theme.qml
        Label.qml
        Button.qml
        ToolButton.qml
        TextField.qml
        ComboBox.qml
        ItemDelegate.qml
        TabBar.qml
        TabButton.qml
        ToolBar.qml
        Frame.qml
        Popup.qml
        Dialog.qml
        DialogButtonBox.qml
        RadioButton.qml
        ProgressBar.qml
        ScrollBar.qml
        ToolTip.qml
        WarningButton.qml
    RESOURCES
        fonts/Onest-Regular.ttf
        fonts/Onest-Medium.ttf
        fonts/Onest-SemiBold.ttf
        fonts/Onest-Bold.ttf
        icons/calendar.svg
        icons/weight.svg
        icons/truck.svg
        icons/warehouse.svg
        icons/wallet.svg
        icons/more.svg
        icons/search.svg
        icons/circle-check.svg
        icons/circle-alert.svg
)
target_link_libraries(wms_style PUBLIC Qt6::Quick Qt6::QuickControls2)
