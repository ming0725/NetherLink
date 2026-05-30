# JKQtPlotter Local Modifications

This directory contains a vendored, reduced, and modified copy of JKQtPlotter
for NetherLink-static.

Upstream project: https://github.com/jkriege2/JKQtPlotter

Modification date: 2026-05-30

Modified files compared with the upstream reference:

- `CMakeLists.txt`
- `cmake/jkqtplotter_cmake_options.cmake`
- `cmake/jkqtplotter_common_compilersettings.cmake`
- `cmake/jkqtplotter_common_include.cmake`
- `cmake/jkqtplotter_common_qtsettings.cmake`
- `cmake/jkqtplotter_macros.cmake`
- `lib/CMakeLists.txt`
- `lib/jkqtmathtext/CMakeLists.txt`
- `lib/jkqtmathtext/nodes/jkqtmathtextbracenode.cpp`
- `lib/jkqtmathtext/nodes/jkqtmathtextsymbolnode.cpp`
- `lib/jkqtmathtext/nodes/jkqtmathtexttextnode.cpp`
- `lib/jkqtmathtext/parsers/jkqtmathtextlatexparser.cpp`
- `lib/jkqtmathtext/parsers/jkqtmathtextlatexparser.h`

Summary of changes:

- Reduced the vendored tree to the JKQTMathText equation renderer and the
  support code needed by NetherLink-static.
- Adapted CMake files for the local vendored build, Qt integration, install
  behavior, and selected build options.
- Adjusted math rendering behavior for double-line braces, integral-like
  symbols, math italic/script Unicode output, forced-upright handling, and
  `\frac`-style parsing of single-token math arguments.

JKQtPlotter remains licensed under the GNU Lesser General Public License,
version 2.1 or later. See `LICENSE` in this directory.
