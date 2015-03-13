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
cmake_minimum_required(VERSION 2.8.0)

SET(CMAKE_INSTALL_PREFIX "/usr")

# Definitions
add_definitions(-std=c++0x)
add_definitions(-Wall)
add_definitions(-Werror)
add_definitions(-Weffc++)
#add_definitions(-pedantic)
add_definitions(-ggdb)
add_definitions(-O0)
#add_definitions(-O2)
#add_definitions(-g3 -fno-omit-frame-pointer -fsanitize=address)
# Package Dependancies
# find_package(Boost REQUIRED)

# Build options
#option(enable_tests "Build the unit test suite." OFF)

# include source and test directories
add_subdirectory(src)

# ------------------------------------------------
# Debian Package Support
# ------------------------------------------------
SET(CPACK_GENERATOR "DEB")
SET(CPACK_DEBIAN_PACKAGE_VERSION "1.1.2")
SET(CPACK_PACKAGE_FILE_NAME "hlo-${CPACK_DEBIAN_PACKAGE_VERSION}-Linux")
SET(CPACK_DEBIAN_PACKAGE_NAME "hlo")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Reed Morrison")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "HTTP Load Tester.")
SET(CPACK_PACKAGE_DESCRIPTION "HTTP Load Tester for Verizon EdgeCast.")
SET(CPACK_DEBIAN_PACKAGE_DEPENDS "google-perftools, libssl1.0.0, libsnappy1, libleveldb1 (>=1.15.0-2), libmicrohttpd5 | libmicrohttpd10")

# cpack doesn't actually use the following, but the build.sh command does
SET(CPACK_DEBIAN_PACKAGE_BUILDS_DEPENDS "libgoogle-perftools-dev, libssl-dev, libsnappy-dev, libleveldb-dev (>=1.15.0-2), libmicrohttpd-dev")

SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
INCLUDE(CPack)

