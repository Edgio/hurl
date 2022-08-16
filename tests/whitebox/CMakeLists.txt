# ------------------------------------------------------------------------------
# build catch
# ------------------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ../../ext/)
add_library(compiled_catch STATIC ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/catch/catch.hpp _catch.cc)
# ------------------------------------------------------------------------------
# build tests
# ------------------------------------------------------------------------------
add_subdirectory(support)
add_subdirectory(dns)
add_subdirectory(nconn)
