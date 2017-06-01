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
# wb_nbq
# ------------------------------------------------
SET(SRCS wb_nbq.cc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/../
                    ../../../include/
                    ../../../src/core/ndebug/)
add_executable(wb_nbq ${HDRS} ${SRCS})
target_link_libraries(wb_nbq compiled_catch  ${CMAKE_BINARY_DIR}/src/core/libhurlcore.a ${LIBRARIES})
SET_TARGET_PROPERTIES(wb_nbq PROPERTIES OUTPUT_NAME "wb_nbq")
add_test(nbq wb_nbq)


# ------------------------------------------------
# wb_obj_pool
# ------------------------------------------------
SET(SRCS wb_obj_pool.cc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/../
                    ../../../src/core/support/
                    ../../../src/core/ndebug/)
add_executable(wb_obj_pool ${HDRS} ${SRCS})
target_link_libraries(wb_obj_pool compiled_catch  ${CMAKE_BINARY_DIR}/src/core/libhurlcore.a ${LIBRARIES})
SET_TARGET_PROPERTIES(wb_obj_pool PROPERTIES OUTPUT_NAME "wb_obj_pool")
add_test(obj_pool wb_obj_pool)

# ------------------------------------------------
# wb_nlru
# ------------------------------------------------
SET(SRCS wb_nlru.cc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/../
                    ../../../src/core/support/
                    ../../../src/core/ndebug/)
add_executable(wb_nlru ${HDRS} ${SRCS})
target_link_libraries(wb_nlru compiled_catch  ${CMAKE_BINARY_DIR}/src/core/libhurlcore.a ${LIBRARIES})
SET_TARGET_PROPERTIES(wb_nlru PROPERTIES OUTPUT_NAME "wb_nlru")
add_test(nlru wb_nlru)
