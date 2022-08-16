# ------------------------------------------------------------------------------
# build catch
# ------------------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ../../ext/)
add_library(compiled_catch STATIC ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/catch/catch.hpp _catch.cc)
# ------------------------------------------------------------------------------
# fix for error
# ...
# /usr/bin/ld: /tmp/ccJLmlSQ.ltrans0.ltrans.o: relocation R_X86_64_32S against
# `.rodata' can not be used when making a PIE object; recompile with -fPIE
# ...
# ref:
# https://github.com/kermitt2/pdfalto/issues/41#issuecomment-479881065
# ------------------------------------------------------------------------------
set(CMAKE_EXE_LINKER_FLAGS "-no-pie")
# ------------------------------------------------------------------------------
# build tests
# ------------------------------------------------------------------------------
add_subdirectory(support)
add_subdirectory(dns)
add_subdirectory(nconn)
