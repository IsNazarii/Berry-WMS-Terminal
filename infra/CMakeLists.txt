# Infrastructure: storage and devices behind the core's interfaces.
# SQLite comes as the official amalgamation (one C file), so building needs no
# system sqlite-devel and every machine gets the same version.
enable_language(C)

include(FetchContent)
FetchContent_Declare(
    sqlite
    URL https://www.sqlite.org/2026/sqlite-amalgamation-3510200.zip
    URL_HASH SHA256=6e2a845a493026bdbad0618b2b5a0cf48584faab47384480ed9f592d912f23ec
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(sqlite)

find_package(Threads REQUIRED)

add_library(sqlite3 STATIC ${sqlite_SOURCE_DIR}/sqlite3.c)
# SYSTEM: our warning flags must not fire on sqlite's own code via its header.
target_include_directories(sqlite3 SYSTEM PUBLIC ${sqlite_SOURCE_DIR})
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_DQS=0                 # "text" is an identifier only; typos fail instead of becoming strings
    SQLITE_OMIT_LOAD_EXTENSION   # no extensions, so no libdl
    SQLITE_DEFAULT_MEMSTATUS=0
)
# Third-party code as released: newer compilers warn about it, and fixing it is not ours to do.
target_compile_options(sqlite3 PRIVATE -w)
target_link_libraries(sqlite3 PRIVATE Threads::Threads)

add_library(wms_infra
    src/sqlite.cpp
    src/schema.cpp
    src/rows.cpp
    src/sqlite_journal_store.cpp
    src/sqlite_session_store.cpp
    src/sqlite_repository.cpp
    src/backup.cpp
)
target_include_directories(wms_infra PUBLIC include)
target_link_libraries(wms_infra PUBLIC wms_core PRIVATE sqlite3)
target_compile_options(wms_infra PRIVATE -Wall -Wextra -Wpedantic -Wshadow)

# The scale's serial port is the only Qt piece of infra; a library of its own keeps
# wms_infra (SQLite) and its tests free of Qt.
add_library(wms_serial src/serial_scale_driver.cpp)
target_include_directories(wms_serial PUBLIC include)
target_link_libraries(wms_serial PUBLIC wms_core Qt6::SerialPort)
target_compile_options(wms_serial PRIVATE -Wall -Wextra -Wpedantic -Wshadow)

# QR codes: nayuki's generator has no build files of its own, its two C++ files are the library.
FetchContent_Declare(
    qrcodegen
    URL https://github.com/nayuki/QR-Code-generator/archive/refs/tags/v1.8.0.tar.gz
    URL_HASH SHA256=2ec0a4d33d6f521c942eeaf473d42d5fe139abcfa57d2beffe10c5cf7d34ae60
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(qrcodegen)

add_library(qrcodegen STATIC ${qrcodegen_SOURCE_DIR}/cpp/qrcodegen.cpp)
target_include_directories(qrcodegen SYSTEM PUBLIC ${qrcodegen_SOURCE_DIR}/cpp)
target_compile_options(qrcodegen PRIVATE -w)

# The printed receipt: Qt lays it out and prints it, qrcodegen makes its QR code.
add_library(wms_print
    src/qr_code.cpp
    src/receipt_printer.cpp
)
target_include_directories(wms_print PUBLIC include)
target_link_libraries(wms_print PUBLIC wms_core Qt6::PrintSupport PRIVATE qrcodegen)
target_compile_options(wms_print PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
