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
    ./evr
    ./support
    ./nconn
    ./dns
    ./http
    ../../include
    ${LIBUDNS_INCLUDE_DIR}
)

# ------------------------------------------------
# Add udns object files manually for OS X
# ------------------------------------------------
IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    SET(UDNS_SRCS
        ../../../ext/udns-0.4/dnsget.o
        ../../../ext/udns-0.4/rblcheck.o
        ../../../ext/udns-0.4/udns_codes.o
        ../../../ext/udns-0.4/udns_dntosp.o
        ../../../ext/udns-0.4/udns_jran.o
        ../../../ext/udns-0.4/udns_parse.o
        ../../../ext/udns-0.4/udns_rr_a.o
        ../../../ext/udns-0.4/udns_rr_naptr.o
        ../../../ext/udns-0.4/udns_rr_srv.o
        ../../../ext/udns-0.4/udns_XtoX.o
        ../../../ext/udns-0.4/ex-rdns.o
        ../../../ext/udns-0.4/udns_bl.o
        ../../../ext/udns-0.4/udns_dn.o
        ../../../ext/udns-0.4/udns_init.o
        ../../../ext/udns-0.4/udns_misc.o
        ../../../ext/udns-0.4/udns_resolver.o
        ../../../ext/udns-0.4/udns_rr_mx.o
        ../../../ext/udns-0.4/udns_rr_ptr.o
        ../../../ext/udns-0.4/udns_rr_txt.o
    )
    string (REPLACE ";" " " UDNS_SRCS_FIXED "${UDNS_SRCS}")
    set (CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} ${UDNS_SRCS_FIXED}")
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${UDNS_SRCS_FIXED}")
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

# ------------------------------------------------
# OS X Specific
# ------------------------------------------------
IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    include_directories(/usr/local/opt/openssl/include)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

# Source files
SET(SRCS
    ../../ext/http_parser/http_parser.cc
    ../../ext/hostcheck/hostcheck.cc
    ../../ext/base64/base64.cc
    
    ./ndebug/ndebug.cc
    
    ./evr/evr.cc
    ./evr/evr_select.cc
    
    ./support/string_util.cc
    ./support/time_util.cc
    ./support/tls_util.cc
    ./support/uri.cc
    ./support/nbq.cc
    ./support/trace.cc

    ./nconn/nconn.cc
    ./nconn/nconn_tcp.cc
    ./nconn/nconn_tls.cc
    ./nconn/nconn_pool.cc
    ./nconn/host_info.cc
    
    ./dns/ai_cache.cc
    ./dns/nresolver.cc
    ./dns/nlookup.cc
    
    ./http/hmsg.cc
    ./http/rqst.cc
    ./http/resp.cc
    ./http/cb.cc
    ./http/api_resp.cc

)

# ------------------------------------------------
# Linux Specific
# ------------------------------------------------
IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    # Linux specific code
    SET(OperatingSystem "Linux")
    set (SRCS ${SRCS} ./evr/evr_epoll.cc)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

# Headers
SET(HDRS
    ../../include/hlx/evr/evr.h

    ../../include/hlx/support/atomic.h
    ../../include/hlx/support/kv_map_list.h
    ../../include/hlx/support/time_util.h
    ../../include/hlx/support/trace.h

    ../../include/hlx/nconn/scheme.h
    ../../include/hlx/nconn/conn_status.h
    ../../include/hlx/nconn/host_info.h

    ../../include/hlx/http/cr.h
    ../../include/hlx/http/http_status.h
    ../../include/hlx/http/hmsg.h
    ../../include/hlx/http/rqst.h
    ../../include/hlx/http/resp.h
    ../../include/hlx/http/api_resp.h

)

add_library(hlxcore STATIC ${HDRS} ${SRCS})

if(BUILD_UDNS)
  add_dependencies(hlxcore ext_udns)
endif()
  
target_link_libraries(hlxcore UDNS_OBJS_OBJECT)

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
IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  add_custom_command(
    TARGET hlxcore
    POST_BUILD
    COMMAND ar -M < ${CMAKE_SOURCE_DIR}/src/core/libhlx.mri && mv libhlx.a libhlxcore.a
    DEPENDS hlxcore udns
  )
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

