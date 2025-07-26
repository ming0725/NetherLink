cmake_minimum_required(VERSION 3.10)

project(NetherLink-Client VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network WebSockets Sql Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network WebSockets Sql Concurrent)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

file(GLOB_RECURSE INCLUDES "include/*.h" "include/**/*.h")
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/**/*.cpp")

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(NetherLink-Client
        MANUAL_FINALIZATION
        ${INCLUDES}
        ${SOURCES}
        res/res.qrc
    )
endif()

target_link_libraries(NetherLink-Client PRIVATE
        Qt6::Widgets
        Qt6::Network
        Qt6::WebSockets
        Qt6::Sql
        Qt6::Concurrent
        dwmapi
        user32
)

target_compile_options(NetherLink-Client PRIVATE
    -Wall
    -Wextra
    # -Werror
    -Wbuiltin-macro-redefined
    -Wunused-variable
    -Wunused-function
    -pedantic
    -O3
)

set_target_properties(NetherLink-Client PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

install(TARGETS NetherLink-Client
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(NetherLink-Client)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/Debug")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/Debug")
else(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/Release")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/Release")
endif()
