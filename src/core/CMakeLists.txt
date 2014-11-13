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

# include paths
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
    #${CMAKE_CURRENT_BINARY_DIR}
    ./ndebug
    ./http_parser
    ./tinymt
    ./evr
    ./cityhash
    ./support
)

# Source files
SET(SRCS
    ./ndebug/ndebug.cc
    ./http_parser/http_parser.cc
    ./tinymt/tinymt64.cc
    ./evr/evr.cc
    ./evr/evr_epoll.cc
    ./evr/evr_select.cc
    ./cityhash/city.cc
    ./support/util.cc
    ./support/parsed_url.cc
    ./support/reqlet.cc
    ./support/nconn.cc
    ./support/resolver.cc
)

# Headers
SET(HDRS
    ./ndebug/ndebug.h
    ./tinymt/tinymt64.h
)

add_library(hlxcore STATIC ${HDRS} ${SRCS})

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
#install(FILES ${HDRS}
#  DESTINATION include/hlo
#  COMPONENT Headers
#)

