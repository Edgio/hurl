# ----------------------------------------------------------------------------
# Copyright (C) 2017 Verizon.  All Rights Reserved.
# All Rights Reserved
#
#   Author: Reed P Morrison
#   Date:   04/08/2017
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
# ------------------------------------------------------------------------------
# ------------------------------------------------------------------------------
# Project setup
# ------------------------------------------------------------------------------
project(hurl)
cmake_minimum_required(VERSION 2.8.0)
if (POLICY CMP0050)
    cmake_policy(SET CMP0050 OLD)
endif()
# ------------------------------------------------------------------------------
# sources
# ------------------------------------------------------------------------------
# include paths
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/ext
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/ext/nghttp2/include
)
SET(SRCS h2url.cc)
SET(HDRS)
# ------------------------------------------------------------------------------
# Man page...
# ------------------------------------------------------------------------------
ADD_CUSTOM_TARGET(h2url_man ALL)
ADD_CUSTOM_COMMAND(
  TARGET h2url_man
  SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/h2url.pod
  COMMAND pod2man ARGS -r "VDMS" -s 1 -c "h2url manual" ${CMAKE_CURRENT_SOURCE_DIR}/h2url.pod ${CMAKE_CURRENT_BINARY_DIR}/h2url.1
  OUTPUTS ${CMAKE_CURRENT_BINARY_DIR}/h2url.1
)
ADD_CUSTOM_COMMAND(
  TARGET h2url_man
  SOURCE h2url_man
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/h2url.1
)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/h2url.1 DESTINATION ${CMAKE_INSTALL_PREFIX}/man/man1)
# ------------------------------------------------------------------------------
# build and install
# ------------------------------------------------------------------------------
add_executable(h2url_cmd ${HDRS} ${SRCS})
add_dependencies(h2url_cmd hurlcore)
add_dependencies(h2url_cmd nghttp2)
SET_TARGET_PROPERTIES(h2url_cmd
  PROPERTIES OUTPUT_NAME "h2url"
)
target_link_libraries(h2url_cmd
  ${CMAKE_BINARY_DIR}/ext/nghttp2/src/libnghttp2.a
  ${CMAKE_BINARY_DIR}/src/core/libhurlcore.a
  ${CMAKE_SOURCE_DIR}/ext/udns-0.4/libudns.a
  ${LIBRARIES}
)
install(TARGETS
  h2url_cmd
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/
  COMPONENT Executables
)
