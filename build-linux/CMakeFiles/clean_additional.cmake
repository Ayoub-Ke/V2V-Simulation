# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles/V2VSimulation_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/V2VSimulation_autogen.dir/ParseCache.txt"
  "V2VSimulation_autogen"
  )
endif()
