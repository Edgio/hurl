# ------------------------------------------------------------------------------
# include paths
# ------------------------------------------------------------------------------
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/ext
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/ext/nghttp2/include
)
# ------------------------------------------------------------------------------
# sources
# ------------------------------------------------------------------------------
SET(SRCS hurl.cc ${CMAKE_SOURCE_DIR}/ext/tinymt/tinymt64.cc)
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
# build and install
# ------------------------------------------------------------------------------
add_executable(hurl_cmd ${HDRS} ${SRCS})
add_dependencies(hurl_cmd hurlcore)
SET_TARGET_PROPERTIES(hurl_cmd PROPERTIES OUTPUT_NAME "hurl")
target_link_libraries(hurl_cmd
  ${DEBUG_LIBRARIES}
  ${CMAKE_BINARY_DIR}/ext/nghttp2/src/libnghttp2.a
  ${CMAKE_BINARY_DIR}/src/core/libhurlcore.a
  ${CMAKE_SOURCE_DIR}/ext/udns-0.4/libudns.a
  ${LIBRARIES}
)
install(TARGETS hurl_cmd
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/
  COMPONENT Executables
)
