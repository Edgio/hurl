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

# Set install prefix
SET(CMAKE_INSTALL_PREFIX "/usr")

# Build options
option(GCC_OPTIONS    "Command line options passed to gcc or 'native' to compile for this hardware" OFF)
option(DEBUG_MODE     "Compile in debug mode." OFF)
option(BUILD_TESTS    "Build the unit tests." OFF)
#option(BUILD_PROFILE  "Enable google cpu and heap profiler support" OFF)
#option(BUILD_TCMALLOC "Build with tcmalloc" ON)

# ------------------------------------------------------------------------------
# Compiler options
# ------------------------------------------------------------------------------
add_definitions(-std=c++0x)
add_definitions(-Wall -Werror -Weffc++ -Wformat -Wformat-security)
#add_definitions(-pedantic)
#add_definitions(-D_FORTIFY_SOURCE=2 -O1 -Wl,-z,relro,-z,now)

# Address sanitizer options
#add_definitions(-g3 -fno-omit-frame-pointer -fsanitize=address)

# ------------------------------------------------------------------------------
# Optional flags
# ------------------------------------------------------------------------------
if (DEBUG_MODE)
  add_definitions(-O0 -ggdb)
else()
  add_definitions(-O2)
endif()

# ------------------------------------------------------------------------------
# Display the current settings
# ------------------------------------------------------------------------------
message(STATUS "Build Configuration:")
message("")
message("    Build Option                               Variable                Value                 ")
message("    ------------------------------------------------------------------------------------------")
message("    Debug mode:                                " "DEBUG_MODE              " ${DEBUG_MODE})
message("    Build unit tests:                          " "BUILD_TESTS             " ${BUILD_TESTS})
message("    Install path:                              " "INSTALL_PREFIX          " ${CMAKE_INSTALL_PREFIX})
message("")

# ------------------------------------------------------------------------------
# include source and test directories
# ------------------------------------------------------------------------------
add_subdirectory(src)

if (BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ------------------------------------------------------------------------------
# Debian Package Support
# ------------------------------------------------------------------------------
EXECUTE_PROCESS(COMMAND lsb_release -cs OUTPUT_VARIABLE DISTRIBUTION OUTPUT_STRIP_TRAILING_WHITESPACE)
SET(CPACK_GENERATOR "DEB")
SET(CPACK_DEBIAN_PACKAGE_VERSION "1.3.0-${DISTRIBUTION}")
SET(CPACK_PACKAGE_FILE_NAME "hlo-${CPACK_DEBIAN_PACKAGE_VERSION}-Linux")
SET(CPACK_DEBIAN_PACKAGE_NAME "hlo")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Reed Morrison")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "HTTP Load Tester.")
SET(CPACK_PACKAGE_DESCRIPTION "HTTP Load Tester for Verizon EdgeCast.")
SET(CPACK_DEBIAN_PACKAGE_DEPENDS "google-perftools, libssl1.0.0")

# cpack doesn't actually use the following, but the build.sh command does
SET(CPACK_DEBIAN_PACKAGE_BUILDS_DEPENDS "libgoogle-perftools-dev, libssl-dev")

SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
INCLUDE(CPack)
