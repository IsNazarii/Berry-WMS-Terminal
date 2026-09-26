# QML screens and thin Qt adapters over the core. No business rules here.
qt_add_library(wms_ui STATIC)
qt_add_qml_module(wms_ui
    URI BerryWms.Ui
    SOURCES
        src/terminal.h
        src/terminal.cpp
        src/supplier_list_model.h
        src/supplier_list_model.cpp
        src/scale_monitor.h
        src/scale_monitor.cpp
    QML_FILES
        qml/Main.qml
        qml/screens/LoginScreen.qml
        qml/screens/SessionScreen.qml
        qml/screens/ReceivingScreen.qml
        qml/screens/SupplierPicker.qml
        qml/screens/AllocationScreen.qml
        qml/screens/CashScreen.qml
)

# The generated QML type registration includes headers by bare name.
target_include_directories(wms_ui PUBLIC src)
target_link_libraries(wms_ui PUBLIC Qt6::Quick Qt6::QuickControls2 wms_core)
target_compile_options(wms_ui PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
