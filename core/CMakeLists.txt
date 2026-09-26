# Domain core: pure C++20, no Qt. Everything here must be unit-testable.
add_library(wms_core
    src/quantity.cpp
    src/uuid.cpp
    src/text.cpp
    src/supplier.cpp
    src/transaction.cpp
    src/receipt.cpp
    src/payment.cpp
    src/scale.cpp
    src/truck.cpp
    src/allocation.cpp
    src/journal.cpp
)

target_include_directories(wms_core PUBLIC include)
target_compile_options(wms_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
