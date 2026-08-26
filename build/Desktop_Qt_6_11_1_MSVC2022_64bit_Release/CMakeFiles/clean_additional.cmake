# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles\\matbin_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\matbin_autogen.dir\\ParseCache.txt"
  "matbin_autogen"
  "third_party\\matio\\CMakeFiles\\getopt_autogen.dir\\AutogenUsed.txt"
  "third_party\\matio\\CMakeFiles\\getopt_autogen.dir\\ParseCache.txt"
  "third_party\\matio\\CMakeFiles\\matdump_autogen.dir\\AutogenUsed.txt"
  "third_party\\matio\\CMakeFiles\\matdump_autogen.dir\\ParseCache.txt"
  "third_party\\matio\\CMakeFiles\\matio_autogen.dir\\AutogenUsed.txt"
  "third_party\\matio\\CMakeFiles\\matio_autogen.dir\\ParseCache.txt"
  "third_party\\matio\\getopt_autogen"
  "third_party\\matio\\matdump_autogen"
  "third_party\\matio\\matio_autogen"
  )
endif()
