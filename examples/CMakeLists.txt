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
project(hlx_examples)
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
target_link_libraries(basic ${CMAKE_BINARY_DIR}/src/core/libhlxcore.a ${LIBRARIES})

# ----------------------------------------------------------
# subrequest
# ----------------------------------------------------------
add_executable(subrequest subrequest.cc)
add_dependencies(subrequest hlxcore)
SET_TARGET_PROPERTIES(subrequest PROPERTIES OUTPUT_NAME "subrequest")
target_link_libraries(subrequest ${CMAKE_BINARY_DIR}/src/core/libhlxcore.a ${LIBRARIES})

# ----------------------------------------------------------
# floodecho
# ----------------------------------------------------------
add_executable(floodecho floodecho.cc)
add_dependencies(floodecho hlxcore)
SET_TARGET_PROPERTIES(floodecho PROPERTIES OUTPUT_NAME "floodecho")
target_link_libraries(floodecho ${CMAKE_BINARY_DIR}/src/core/libhlxcore.a ${LIBRARIES})

# ----------------------------------------------------------
# files
# ----------------------------------------------------------
add_executable(files files.cc)
add_dependencies(files hlxcore)
SET_TARGET_PROPERTIES(files PROPERTIES OUTPUT_NAME "files")
target_link_libraries(files ${CMAKE_BINARY_DIR}/src/core/libhlxcore.a ${LIBRARIES})
