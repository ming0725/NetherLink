cmake_minimum_required(VERSION 3.23)

project(NetherLink VERSION 0.1 LANGUAGES CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets OpenGL Concurrent Svg Xml Sql Network WebSockets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets OpenGL Concurrent Svg Xml Sql Network WebSockets)
find_package(Threads REQUIRED)

set(JKQtPlotter_BUILD_LIB_JKQTMATHTEXT ON CACHE BOOL "" FORCE)
set(JKQtPlotter_BUILD_INCLUDE_XITS_FONTS ON CACHE BOOL "" FORCE)
set(JKQtPlotter_BUILD_WITH_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/JKQtPlotter EXCLUDE_FROM_ALL)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/*.h"
)
file(GLOB_RECURSE FEATURE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/features/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/features/*.h"
)
file(GLOB_RECURSE SHARED_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/shared/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/shared/*.h"
)
list(REMOVE_ITEM SHARED_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/shared/theme/AppFonts.cpp"
)
file(GLOB_RECURSE PLATFORM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/platform/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/platform/*.h"
)

if(APPLE)
    file(GLOB_RECURSE PLATFORM_OBJCXX_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/*.mm"
    )
    list(APPEND PLATFORM_SOURCES ${PLATFORM_OBJCXX_SOURCES})
endif()

set(PROJECT_SOURCES
    main.cpp
    resources.qrc
    resources/audio/portal2.wav
    third_party/miniaudio/miniaudio.cpp
    ${APP_SOURCES}
    ${FEATURE_SOURCES}
    ${SHARED_SOURCES}
    ${PLATFORM_SOURCES}
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${PROJECT_SOURCES})

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(NetherLink
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET NetherLink APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(NetherLink SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(NetherLink
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_include_directories(NetherLink PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/third_party/miniaudio
)

target_compile_definitions(NetherLink PRIVATE
    QMARKDOWN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shared/ui/markdown"
)

target_link_libraries(NetherLink PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::OpenGL
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::Svg
    Qt${QT_VERSION_MAJOR}::Xml
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::WebSockets
    JKQTPlotter${QT_VERSION_MAJOR}::JKQTMathText${QT_VERSION_MAJOR}
    Threads::Threads
)

add_custom_command(TARGET NetherLink POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shared/ui/markdown/syntax"
        "$<TARGET_FILE_DIR:NetherLink>/syntax"
)

if(UNIX AND NOT APPLE)
    target_link_libraries(NetherLink PRIVATE ${CMAKE_DL_LIBS} m)
endif()

if(APPLE)
    target_link_libraries(NetherLink PRIVATE "-framework QuartzCore" "-framework CoreFoundation")
endif()

if(WIN32)
    target_link_libraries(NetherLink PRIVATE dwmapi user32)
endif()

set_target_properties(NetherLink PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

install(TARGETS NetherLink
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(NetherLink)
endif()
