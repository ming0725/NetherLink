# Modified by NetherLink-static contributors on 2026-05-30.
# Changes: reduced library build to JKQTMathText and required common support;
# see ../MODIFICATIONS.md.
cmake_minimum_required(VERSION 3.23)

message(STATUS)
message(STATUS "=============================================================================")
message(STATUS "== JKQTMathText Build Information                                          ==")
message(STATUS "=============================================================================")
message(STATUS "Version:               ${PROJECT_VERSION}")
message(STATUS "Build shared lib:      ${BUILD_SHARED_LIBS}")
message(STATUS "Using CMake:           ${CMAKE_VERSION}")
message(STATUS "Using compiler:        ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Using Qt:              ${QT_VERSION}")
message(STATUS "XITS fonts:            ${JKQtPlotter_BUILD_INCLUDE_XITS_FONTS}")
message(STATUS "=============================================================================")
message(STATUS)

set(JKQTPLOTTER_VERSION_FILE_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(JKQTPLOTTER_VERSION_FILE ${JKQTPLOTTER_VERSION_FILE_DIR}/jkqtplotter_version.h)
configure_file(${CMAKE_CURRENT_LIST_DIR}/jkqtplotter_version.h.in ${JKQTPLOTTER_VERSION_FILE})

include_directories(.)

add_subdirectory(jkqtcommon)
add_subdirectory(jkqtmathtext)
