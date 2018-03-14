# ----------------------------------------------------------------------------
# Copyright (C) 2014 Verizon.  All Rights Reserved.
# All Rights Reserved
#
#   Author: Reed P Morrison
#   Date:   02/07/2014
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
# ------------------------------------------------
# Project setup
# ------------------------------------------------
project(hurl)
cmake_minimum_required(VERSION 2.6.0)
# ------------------------------------------------
# include paths
# ------------------------------------------------
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/ext
    ${LIBUDNS_INCLUDE_DIR}
)
# ------------------------------------------------
# OS X Specific
# ------------------------------------------------
IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    include_directories(/usr/local/opt/openssl/include)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
# ------------------------------------------------
# Source files
# ------------------------------------------------
SET(SRCS
    ${CMAKE_SOURCE_DIR}/ext/http_parser/http_parser.cc
    ${CMAKE_SOURCE_DIR}/ext/hostcheck/hostcheck.cc
    ${CMAKE_SOURCE_DIR}/ext/base64/base64.cc
    ./evr/evr.cc
    ./evr/evr_select.cc
    ./support/ndebug.cc
    ./support/file_util.cc
    ./support/string_util.cc
    ./support/time_util.cc
    ./support/tls_util.cc
    ./support/uri.cc
    ./support/nbq.cc
    ./support/trace.cc
    ./nconn/nconn.cc
    ./nconn/nconn_tcp.cc
    ./nconn/nconn_tls.cc
    ./nconn/host_info.cc
    ./dns/ai_cache.cc
    ./dns/nresolver.cc
    ./dns/nlookup.cc
    ./http/hmsg.cc
    ./http/rqst.cc
    ./http/resp.cc
    ./http/cb.cc
)
# ------------------------------------------------
# Linux Specific
# ------------------------------------------------
IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    # Linux specific code
    SET(OperatingSystem "Linux")
    set (SRCS ${SRCS} ./evr/evr_epoll.cc)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
# ------------------------------------------------
# Headers
# ------------------------------------------------
SET(HDRS)
# ------------------------------------------------
# library
# ------------------------------------------------
add_library(hurlcore STATIC ${HDRS} ${SRCS})
if(BUILD_UDNS)
  add_dependencies(hurlcore ext_udns)
endif()
target_link_libraries(hurlcore UDNS_OBJS_OBJECT)
# Create symlink for libhurlcore.so.<soversion>
SET_TARGET_PROPERTIES(hurlcore
  PROPERTIES
  SOVERSION 0
)
