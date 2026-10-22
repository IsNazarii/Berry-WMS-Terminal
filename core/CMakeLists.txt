# Domain core: pure C++20, no Qt. Everything here must be unit-testable.
add_library(wms_core
    src/quantity.cpp
    src/uuid.cpp
    src/text.cpp
    src/supplier.cpp
    src/crop.cpp
    src/operator.cpp
    src/transaction.cpp
    src/receipt.cpp
    src/open_receipt.cpp
    src/payment.cpp
    src/scale.cpp
    src/truck.cpp
    src/allocation.cpp
    src/shipment.cpp
    src/journal.cpp
    src/pricing.cpp
    src/workday.cpp
)

target_include_directories(wms_core PUBLIC include)
target_compile_options(wms_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
