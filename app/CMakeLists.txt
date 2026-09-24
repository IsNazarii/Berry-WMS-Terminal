# Single entry point (course requirement). Console demo until the Qt/QML UI is added.
add_executable(wms_terminal main.cpp)
target_link_libraries(wms_terminal PRIVATE wms_core)
target_compile_options(wms_terminal PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
