include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

add_executable(wms_core_tests
    quantity_test.cpp
    uuid_test.cpp
    supplier_test.cpp
    crop_test.cpp
    crop_keys_test.cpp
    transaction_test.cpp
    scale_test.cpp
    ie04_test.cpp
    truck_test.cpp
    allocation_test.cpp
    journal_test.cpp
    pricing_test.cpp
    workday_test.cpp
    shipment_test.cpp
    operator_test.cpp
    open_receipt_test.cpp
    session_summary_test.cpp
    receipt_document_test.cpp
)
target_link_libraries(wms_core_tests PRIVATE wms_core GTest::gtest_main)
# Raw bytes recorded from real devices.
target_compile_definitions(wms_core_tests PRIVATE WMS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

# Infrastructure tests run against real (in-memory or temporary) databases.
add_executable(wms_infra_tests
    sqlite_test.cpp
    sqlite_journal_store_test.cpp
    sqlite_session_store_test.cpp
    sqlite_repository_test.cpp
    backup_test.cpp
)
target_link_libraries(wms_infra_tests PRIVATE wms_infra GTest::gtest_main)

# The serial scale driver against a pseudo-terminal playing the scale; own main()
# because QSerialPort needs a QCoreApplication.
add_executable(wms_serial_tests serial_scale_driver_test.cpp)
target_link_libraries(wms_serial_tests PRIVATE wms_serial GTest::gtest)
target_compile_definitions(wms_serial_tests PRIVATE WMS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

# The receipt drawn on an image and printed to PDF; own main() because fonts need a
# QGuiApplication. Never touches a real printer.
add_executable(wms_print_tests receipt_printer_test.cpp)
target_link_libraries(wms_print_tests PRIVATE wms_print GTest::gtest)
target_compile_definitions(wms_print_tests PRIVATE WMS_FONT_DIR="${CMAKE_SOURCE_DIR}/ui/style/fonts")

include(GoogleTest)
gtest_discover_tests(wms_core_tests)
gtest_discover_tests(wms_infra_tests)
gtest_discover_tests(wms_serial_tests)
gtest_discover_tests(wms_print_tests)
