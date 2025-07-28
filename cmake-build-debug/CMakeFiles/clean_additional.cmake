# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\NetherLink-Client_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\NetherLink-Client_autogen.dir\\ParseCache.txt"
  "NetherLink-Client_autogen"
  )
endif()
