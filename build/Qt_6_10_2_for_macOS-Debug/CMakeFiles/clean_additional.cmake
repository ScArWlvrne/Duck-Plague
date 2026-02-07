# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/DuckPlague_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/DuckPlague_autogen.dir/ParseCache.txt"
  "DuckPlague_autogen"
  )
endif()
