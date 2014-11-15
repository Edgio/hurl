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
project(hlo)
cmake_minimum_required(VERSION 2.6.0)

# header files to install
LIST(APPEND INST_HDRS ${HDRS})

# include paths
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
#${CMAKE_CURRENT_BINARY_DIR}
    ../core/support/
    ../core/evr/
    ../core/ndebug
    ../core/http_parser
    ../core/tinymt
    ../core/cityhash
)

#add_definitions(-std=c++0x)

# Source files
SET(SRCS
  main.cc
  t_microhttpd.cc
  stats_collector.cc
  t_client.cc
  hlo.cc
)

# Headers
SET(HDRS
  t_microhttpd.h
  stats_collector.h
  t_client.h
  hlo.h
)

# ============================================================
# TODO
# ============================================================
## Platform-specific executable targets
#IF(UNIX)
#    IF(APPLE)
#        # MacOS Application Bundle
#        add_executable(hlo MACOSX_BUNDLE ${HDRS} ${SRCS})
#    ELSE()
#        IF(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD|OpenBSD|NetBSD|Solaris|SunOS")
#            # *BSD or Solaris
#            add_executable(hlo ${HDRS} ${SRCS})
#        ELSE()
#            # Linux
#            add_executable(hlo ${HDRS} ${SRCS})
#        ENDIF()
#    ENDIF()
#ELSE()
#    IF(WIN32)
#        # Windows Executable
#        add_executable(hlo ${HDRS} ${SRCS})
#    ENDIF()
#ENDIF()
# ============================================================
# ============================================================


# build and install hl
add_executable(hlo_cmd ${HDRS} ${SRCS})
SET_TARGET_PROPERTIES(hlo_cmd PROPERTIES OUTPUT_NAME "hlo")
target_link_libraries(hlo_cmd
pthread
microhttpd
rt
${CMAKE_BINARY_DIR}/src/core/libhlxcore.a
rt
crypto
ssl
snappy
leveldb
tcmalloc
profiler
#asan
#efence
)

install(TARGETS hlo_cmd
        DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/
        COMPONENT Executables
        )
