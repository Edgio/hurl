# ------------------------------------------------------------------------------
# includes
# ------------------------------------------------------------------------------
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/src/core
  ${CMAKE_SOURCE_DIR}/ext
)
# ------------------------------------------------------------------------------
# build catch
# ------------------------------------------------------------------------------
add_library(compiled_catch STATIC ${CMAKE_SOURCE_DIR}/ext/catch/catch.hpp _catch.cc)
# ------------------------------------------------------------------------------
# macros
# ------------------------------------------------------------------------------
macro (add_hurl_wb_test test_name)
  add_executable("wb_${test_name}" "wb_${test_name}.cc")
  target_link_libraries("wb_${test_name}"
    ${DEBUG_LIBRARIES}
    compiled_catch
    ${CMAKE_BINARY_DIR}/src/core/libhurlcore.a
    ${LIBRARIES})
  add_test(${test_name} "wb_${test_name}")
endmacro()
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
