# ----------------------------------------------------------------------------
# Copyright (C) 2014 Verizon.  All Rights Reserved.
# All Rights Reserved
#
#   Author: Reed P Morrison
#   Date:   09/30/2015  
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
project(hurl)
cmake_minimum_required(VERSION 2.8.0)

# ------------------------------------------------
# wb_ai_cache
# ------------------------------------------------
SET(SRCS wb_ai_cache.cc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/../
                    ../../../ext
                    ../../../include
                    ../../../src/core/dns/
                    ../../../src/core/support/
                    ../../../src/core/ndebug/)
add_executable(wb_ai_cache ${HDRS} ${SRCS})
target_link_libraries(wb_ai_cache
  compiled_catch
  ${CMAKE_BINARY_DIR}/src/core/libhurlcore.a
  ${CMAKE_SOURCE_DIR}/ext/udns-0.4/libudns.a
  ${LIBRARIES}
)
SET_TARGET_PROPERTIES(wb_ai_cache PROPERTIES OUTPUT_NAME "wb_ai_cache")
add_test(ai_cache wb_ai_cache)

# ------------------------------------------------
# wb_nresolver
# ------------------------------------------------
SET(SRCS wb_nresolver.cc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/../
                    ../../../ext
                    ../../../include
                    ../../../src/core/dns/
                    ../../../src/core/evr/
                    ../../../src/core/support/
                    ../../../src/core/ndebug/)
add_executable(wb_nresolver ${HDRS} ${SRCS})
target_link_libraries(wb_nresolver
  compiled_catch
  ${CMAKE_BINARY_DIR}/src/core/libhurlcore.a
  ${CMAKE_SOURCE_DIR}/ext/udns-0.4/libudns.a
  ${LIBRARIES}
)
SET_TARGET_PROPERTIES(wb_nresolver PROPERTIES OUTPUT_NAME "wb_nresolver")
add_test(nresolver wb_nresolver)
