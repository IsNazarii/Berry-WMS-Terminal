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
