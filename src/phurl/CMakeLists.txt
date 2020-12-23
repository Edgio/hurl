# ------------------------------------------------------------------------------
# Project setup
# ------------------------------------------------------------------------------
project(phurl)
cmake_minimum_required(VERSION 2.6.0)
if (POLICY CMP0050)
    cmake_policy(SET CMP0050 OLD)
endif()
LIST(APPEND INST_HDRS ${HDRS})
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/ext
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/ext/nghttp2/include
)
SET(SRCS phurl.cc)
SET(HDRS)
# ------------------------------------------------------------------------------
# Man page...
# ------------------------------------------------------------------------------
ADD_CUSTOM_TARGET(phurl_man ALL)
ADD_CUSTOM_COMMAND(
  TARGET phurl_man
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/phurl.pod
  COMMAND pod2man ARGS -r "VDMS" -s 1 -c "phurl manual" ${CMAKE_CURRENT_SOURCE_DIR}/phurl.pod ${CMAKE_CURRENT_BINARY_DIR}/phurl.1
  OUTPUTS ${CMAKE_CURRENT_BINARY_DIR}/phurl.1
)
ADD_CUSTOM_COMMAND(
  TARGET phurl_man
  SOURCE phurl_man
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/phurl.1
)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/phurl.1 DESTINATION ${CMAKE_INSTALL_PREFIX}/man/man1)
# ------------------------------------------------------------------------------
# build and install
# ------------------------------------------------------------------------------
add_executable(phurl_cmd ${HDRS} ${SRCS})
add_dependencies(phurl_cmd hurlcore)
SET_TARGET_PROPERTIES(phurl_cmd PROPERTIES OUTPUT_NAME "phurl")
target_link_libraries(phurl_cmd
  ${CMAKE_BINARY_DIR}/ext/nghttp2/src/libnghttp2.a
  ${CMAKE_BINARY_DIR}/src/core/libhurlcore.a
  ${CMAKE_SOURCE_DIR}/ext/udns-0.4/libudns.a
  ${LIBRARIES}
)
install(TARGETS phurl_cmd
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/
  COMPONENT Executables
)
