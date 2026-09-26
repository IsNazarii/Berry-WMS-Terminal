# QML screens and thin Qt adapters over the core. No business rules here.
qt_add_library(wms_ui STATIC)
qt_add_qml_module(wms_ui
    URI BerryWms.Ui
    QML_FILES
        qml/Main.qml
        qml/screens/IdentificationScreen.qml
        qml/screens/WeighingScreen.qml
        qml/screens/AllocationScreen.qml
        qml/screens/CashScreen.qml
)

target_link_libraries(wms_ui PUBLIC Qt6::Quick Qt6::QuickControls2 wms_core)
target_compile_options(wms_ui PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
