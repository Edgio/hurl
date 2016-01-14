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

# Project setup
project(hlx)
cmake_minimum_required(VERSION 2.6.0)

# include paths
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../../ext
    ./ndebug
    ./url_router
    ./evr
    ./support
    ./nconn
    ./dns
    ./hlx
    ../../include
    ${LIBUDNS_INCLUDE_DIR}
)

# Source files
SET(SRCS
    ../../ext/http_parser/http_parser.cc
    ../../ext/hostcheck/hostcheck.cc
    ../../ext/base64/base64.cc
    ./ndebug/ndebug.cc
    ./url_router/url_router.cc
    ./evr/evr.cc
    ./evr/evr_epoll.cc
    ./evr/evr_select.cc
    ./support/string_util.cc
    ./support/time_util.cc
    ./support/tls_util.cc
    ./support/stat_util.cc
    ./support/uri.cc
    ./support/nbq.cc
    ./support/file.cc
    ./nconn/nconn.cc
    ./nconn/nconn_tcp.cc
    ./nconn/nconn_tls.cc
    ./nconn/nconn_pool.cc
    ./dns/nresolver.cc
    ./dns/ai_cache.cc
    ./dns/host_info.cc
    ./hlx/hmsg.cc
    ./hlx/rqst.cc
    ./hlx/resp.cc
    ./hlx/cb.cc
    ./hlx/hlx.cc
    ./hlx/t_hlx.cc
    ./hlx/hconn.cc
    ./hlx/lsnr.cc
    ./hlx/subr.cc
    ./hlx/api_resp.cc
    ./hlx/handler/rqst_h.cc
    ./hlx/handler/default_rqst_h.cc
    ./hlx/handler/file_h.cc
    ./hlx/handler/phurl_h.cc
)

# Headers
SET(HDRS
    ../../include/hlx/hlx.h
    ../../include/hlx/stat.h
    ../../include/hlx/phurl_h.h
    ../../include/hlx/file_h.h
    ../../include/hlx/url_router.h
)

add_library(hlxcore STATIC ${HDRS} ${SRCS})

target_link_libraries(hlxcore libudns)

# Create symlink for libhlxcore.so.<soversion>
SET_TARGET_PROPERTIES(hlxcore
  PROPERTIES
  SOVERSION 0
)

# Add install target for the library
install(TARGETS hlxcore
  ARCHIVE DESTINATION lib
  COMPONENT Libraries
)

# Add install target for header files
install(FILES ${HDRS}
  DESTINATION include/hlx
  COMPONENT Headers
)

# ----------------------------------------------------------
# Custom command to link archives together
# ----------------------------------------------------------
add_custom_command(
  TARGET hlxcore
  POST_BUILD
  COMMAND ar -M < ${CMAKE_SOURCE_DIR}/src/core/libhlx.mri && mv libhlx.a libhlxcore.a
  DEPENDS hlxcore udns
)
