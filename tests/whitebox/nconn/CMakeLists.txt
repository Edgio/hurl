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
# wb_nconn_tcp
# ------------------------------------------------
SET(SRCS wb_nconn_tcp.cc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/../
                    ../../../include/
                    ../../../src/core/support/
                    ../../../src/core/evr/
                    ../../../src/core/dns/
                    ../../../src/core/nconn/
                    ../../../src/core/ndebug/)
add_executable(wb_nconn_tcp ${HDRS} ${SRCS})
target_link_libraries(wb_nconn_tcp compiled_catch ${CMAKE_BINARY_DIR}/src/core/libhurlcore.a ${LIBRARIES})
SET_TARGET_PROPERTIES(wb_nconn_tcp PROPERTIES OUTPUT_NAME "wb_nconn_tcp")
add_test(nconn_tcp wb_nconn_tcp)

# ------------------------------------------------
# wb_nconn_tls
# ------------------------------------------------
SET(SRCS wb_nconn_tls.cc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/../
                    ../../../ext
                    ../../../include                    
                    ../../../src/core/support/
                    ../../../src/core/evr/
                    ../../../src/core/dns/
                    ../../../src/core/nconn/
                    ../../../src/core/ndebug/)
add_executable(wb_nconn_tls ${HDRS} ${SRCS})
target_link_libraries(wb_nconn_tls compiled_catch ${CMAKE_BINARY_DIR}/src/core/libhurlcore.a ${LIBRARIES})
SET_TARGET_PROPERTIES(wb_nconn_tls PROPERTIES OUTPUT_NAME "wb_nconn_tls")
add_test(nconn_tls wb_nconn_tls)
