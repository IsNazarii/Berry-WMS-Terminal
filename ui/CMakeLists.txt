# QML screens and thin Qt adapters over the core. No business rules here.
add_subdirectory(style)

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
        src/display_format.h
        src/display_format.cpp
    QML_FILES
        qml/Main.qml
        qml/screens/LoginScreen.qml
        qml/screens/SessionScreen.qml
        qml/screens/ReceivingScreen.qml
        qml/screens/SupplierPicker.qml
        qml/screens/SectionCaption.qml
        qml/screens/FieldCaption.qml
        qml/screens/ColumnHeader.qml
        qml/screens/AllocationScreen.qml
        qml/screens/WarehouseScreen.qml
        qml/screens/CashScreen.qml
    # Screens use Theme; at run time the controls themselves come from the style.
    IMPORTS BerryWms.Style
)

# The generated QML type registration includes headers by bare name.
target_include_directories(wms_ui PUBLIC src)
target_link_libraries(wms_ui PUBLIC Qt6::Quick Qt6::QuickControls2 wms_core wms_style)
target_compile_options(wms_ui PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
