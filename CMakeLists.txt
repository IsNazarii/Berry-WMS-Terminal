cmake_minimum_required(VERSION 3.28)
project(BerryWms VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WMS_BUILD_TESTS "Build unit tests" ON)

add_subdirectory(core)

# Qt is used only by the UI layer and the entry point; core stays Qt-free.
find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)
# QML modules go to build/qml/<URI path>, where qmllint and the engine expect them.
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

add_subdirectory(ui)
add_subdirectory(app)

if(WMS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
