# ----------------------------------------------------------------------------
# Copyright (C) 2014 Verizon.  All Rights Reserved.
# All Rights Reserved
#
#   Author: Reed P Morrison, David Andrews
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
    ../include
)

# ----------------------------------------------------------
# basic
# ----------------------------------------------------------
add_executable(basic basic.cc)
add_dependencies(basic hlxcore)
SET_TARGET_PROPERTIES(basic PROPERTIES OUTPUT_NAME "basic")
target_link_libraries(basic ${CMAKE_BINARY_DIR}/src/core/libhlxcore.a pthread rt ssl crypto tcmalloc)

# ----------------------------------------------------------
# subreq
# ----------------------------------------------------------
add_executable(subreq subreq.cc)
add_dependencies(subreq hlxcore)
SET_TARGET_PROPERTIES(subreq PROPERTIES OUTPUT_NAME "subreq")
target_link_libraries(subreq ${CMAKE_BINARY_DIR}/src/core/libhlxcore.a pthread rt ssl crypto tcmalloc)

# ----------------------------------------------------------
# fanout
# ----------------------------------------------------------
add_executable(fanout fanout.cc)
add_dependencies(fanout hlxcore)
SET_TARGET_PROPERTIES(fanout PROPERTIES OUTPUT_NAME "fanout")
target_link_libraries(fanout ${CMAKE_BINARY_DIR}/src/core/libhlxcore.a pthread rt ssl crypto tcmalloc)

