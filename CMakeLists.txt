cmake_minimum_required(VERSION 3.30.5)

set(QT_FORCE_CMP0156_TO_VALUE NEW)

project(NetherLink-Client VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_definitions(-DUNICODE -D_UNICODE)

set(CMAKE_PREFIX_PATH "D:/Qt/6.10.0/mingw_64")
set(CMAKE_MAKE_PROGRAM "D:/Qt/Tools/Ninja/ninja.exe")

set(CMAKE_AUTOUIC_SEARCH_PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/Util
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/Window
)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network WebSockets Sql Concurrent)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE INCLUDE "include/*.h" "include/*.hpp" "include/**/*.h" "include/**/*.hpp")
file(GLOB_RECURSE SOURCE "src/*.c" "src/*.cpp" "src/**/*.c" "src/**/*.cpp")
file(GLOB_RECURSE UI "ui/*.ui" "ui/**/*.ui")
file(GLOB_RECURSE RESOURCE "res/res.qrc")

qt_add_executable(NetherLink-Client
    MANUAL_FINALIZATION
    ${INCLUDE}
    ${SOURCE}
    ${UI}
    ${RESOURCE}
)

target_link_libraries(NetherLink-Client PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        Qt6::WebSockets
        Qt6::Sql
        Qt6::Concurrent
        # dwmapi
        # user32
)

target_compile_options(NetherLink-Client PRIVATE
    -Wall
    -Wextra
    -Werror
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
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/Release")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/Release")
endif()
