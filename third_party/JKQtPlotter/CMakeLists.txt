# Modified by NetherLink-static contributors on 2026-05-30.
# Changes: reduced vendored JKQtPlotter to the JKQTMathText build needed by
# NetherLink-static; see MODIFICATIONS.md.
cmake_minimum_required(VERSION 3.23)

# Project Name and Version
# starting from 2020: using semantic version number <major>.<minor>.<patch>
#   - bugfixes which do not change the API/ABI lead to an increase in <patch>
#   - backward-compatible API/ABI-changes mean an increasing <minor> version
#   - NON backward-compatible API/ABI-changes mean an increasing <major> version
# relation to older versioning scheme:
#   - v2015.10  <--> 1.0.0
#   - v2018.08  <--> 2.0.0
#   - v2019.11  <--> 3.x.y
#   ==> sematic versioning starts with 4.0.0
project(JKQTPlotter
          LANGUAGES CXX
          VERSION 5.0.0
          HOMEPAGE_URL https://github.com/jkriege2/JKQtPlotter
          DESCRIPTION "vendored JKQTMathText LaTeX equation renderer for Qt"
      )


# set search path for CMake files
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
# Find includes in the build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# error message when user tries to make in-source build!
include(jkqtplotter_noInsourceBuilds)

# Common Includes for JKQtPlotter
include(jkqtplotter_common_include)

# Project Name and Version
include(jkqtplotter_lib_properties)

# CMake options with default values and description texts
include(jkqtplotter_cmake_options)

# compiler settings for this lib
include(jkqtplotter_common_compilersettings)

# include Qt with appropriate options to build this lib
include(jkqtplotter_common_qtsettings)

# additionnal common macros
include(jkqtplotter_macros)

set(jkqtplotter_LIBNAME_VERSION_PART ${QT_VERSION_MAJOR})
# defines the CMake-namespace for all libraries
set(jkqtplotter_namespace JKQTPlotter${jkqtplotter_LIBNAME_VERSION_PART}::)
# defines the subdirectory for CMake-files, when installing
set(jkqtplotter_cmakeSubdir JKQTPlotter${jkqtplotter_LIBNAME_VERSION_PART})



# Only the formula renderer and its small common support library are vendored.
add_subdirectory(lib)

