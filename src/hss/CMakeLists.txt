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
    ../../include
)

#add_definitions(-std=c++0x)

# Source files
SET(SRCS
  main.cc
)

# Headers
SET(HDRS
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


# build and install hss
add_executable(hss_cmd ${HDRS} ${SRCS})
add_dependencies(hss_cmd hlxcore)
SET_TARGET_PROPERTIES(hss_cmd PROPERTIES OUTPUT_NAME "hss")
target_link_libraries(hss_cmd
${CMAKE_BINARY_DIR}/src/core/libhlxcore.a
pthread
rt
ssl
crypto
tcmalloc
profiler
#asan
#efence
#pthread_workqueue
)

install(TARGETS hss_cmd
        DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/
        COMPONENT Executables
        )
