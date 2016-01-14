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
# wb_url_router
# ------------------------------------------------
SET(SRCS wb_url_router.cc
    ../../../src/core/url_router/url_router.cc
    ../../../src/core/ndebug/ndebug.cc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/../
                    ../../../src/core/url_router/
                    ${CMAKE_SOURCE_DIR}/include
                    ../../../src/core/ndebug/)
add_executable(wb_url_router ${HDRS} ${SRCS})
target_link_libraries(wb_url_router compiled_catch ${LIBRARIES})
SET_TARGET_PROPERTIES(wb_url_router PROPERTIES OUTPUT_NAME "wb_url_router")
add_test(url_router wb_url_router)
