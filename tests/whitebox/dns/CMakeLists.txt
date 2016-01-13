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
project(hlx)
cmake_minimum_required(VERSION 2.8.0)

# ------------------------------------------------
# wb_ai_cache
# ------------------------------------------------
SET(SRCS wb_ai_cache.cc
         ../../../src/core/dns/ai_cache.cc
         ../../../src/core/dns/host_info.cc
         ../../../ext/base64/base64.cc
         ../../../src/core/support/time_util.cc
         ../../../src/core/ndebug/ndebug.cc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/../
                    ../../../ext
                    ../../../include
                    ../../../src/core/dns/
                    ../../../src/core/support/
                    ../../../src/core/ndebug/)
add_executable(wb_ai_cache ${HDRS} ${SRCS})
target_link_libraries(wb_ai_cache compiled_catch ${LIBRARIES})
SET_TARGET_PROPERTIES(wb_ai_cache PROPERTIES OUTPUT_NAME "wb_ai_cache")
add_test(ai_cache wb_ai_cache)

# ------------------------------------------------
# wb_nresolver
# ------------------------------------------------
SET(SRCS wb_nresolver.cc
         ../../../src/core/evr/evr.cc
         ../../../src/core/evr/evr_epoll.cc
         ../../../src/core/evr/evr_select.cc
         ../../../src/core/dns/ai_cache.cc
         ../../../src/core/dns/nresolver.cc
         ../../../src/core/dns/host_info.cc
         ../../../ext/base64/base64.cc
         ../../../src/core/support/time_util.cc
         ../../../src/core/ndebug/ndebug.cc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/../
                    ../../../ext
                    ../../../include
                    ../../../src/core/dns/
                    ../../../src/core/evr/
                    ../../../src/core/support/
                    ../../../src/core/ndebug/)
add_executable(wb_nresolver ${HDRS} ${SRCS})
target_link_libraries(wb_nresolver compiled_catch ${LIBRARIES} ${CMAKE_BINARY_DIR}/src/core/libhlxcore.a)
SET_TARGET_PROPERTIES(wb_nresolver PROPERTIES OUTPUT_NAME "wb_nresolver")
add_test(nresolver wb_nresolver)
