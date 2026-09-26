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
    transaction_test.cpp
    scale_test.cpp
    truck_test.cpp
    allocation_test.cpp
    journal_test.cpp
    pricing_test.cpp
)
target_link_libraries(wms_core_tests PRIVATE wms_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(wms_core_tests)
