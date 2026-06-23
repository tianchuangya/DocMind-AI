# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "CMakeFiles\\DocMindAI_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\DocMindAI_autogen.dir\\ParseCache.txt"
  "DocMindAI_autogen"
  )
endif()
