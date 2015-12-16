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
project(hlx)
cmake_minimum_required(VERSION 2.8.0)

# Set install prefix
SET(CMAKE_INSTALL_PREFIX "/usr")

# Build options
option(GCC_OPTIONS      "Command line options passed to gcc or 'native' to compile for this hardware" OFF)
option(FORTIFY          "Fortify Source GCC options" OFF)
option(DEBUG_MODE       "Compile in debug mode." OFF)
option(BUILD_TESTS      "Build the unit tests." ON)
option(BUILD_TCMALLOC   "Build with tcmalloc" ON)
option(BUILD_PROFILER   "Enable google cpu and heap profiler support" OFF)
option(BUILD_EFENCE     "Build with tcmalloc" OFF)
option(BUILD_ASAN       "Build with Address Sanitizer" OFF)
option(BUILD_UDNS       "Build with udns (async dns support)" ON)

# ------------------------------------------------------------------------------
# Compiler options
# ------------------------------------------------------------------------------
add_definitions(-std=c++0x)
add_definitions(-Wall -Werror -Weffc++ -Wformat -Wformat-security)
#add_definitions(-pedantic)

# ------------------------------------------------------------------------------
# Libraries options
# ------------------------------------------------------------------------------
set(LIBRARIES ${LIBRARIES} pthread rt dl)

# ------------------------------------------------------------------------------
# Display the current settings
# ------------------------------------------------------------------------------
message(STATUS "Build Configuration:")
message("")
message("    Build Option                               Variable                Value                 ")
message("    ------------------------------------------------------------------------------------------")
message("    Debug mode:                                " "DEBUG_MODE              " ${DEBUG_MODE})
message("    Fortify Source:                            " "FORTIFY                 " ${FORTIFY})
message("    Build unit tests:                          " "BUILD_TESTS             " ${BUILD_TESTS})
message("    Install path:                              " "INSTALL_PREFIX          " ${CMAKE_INSTALL_PREFIX})
message("    Enable google cpu/heap profiler support:   " "BUILD_PROFILER          " ${BUILD_PROFILER})
message("    Build with tcmalloc:                       " "BUILD_TCMALLOC          " ${BUILD_TCMALLOC})
message("    Build with efence:                         " "BUILD_EFENCE            " ${BUILD_EFENCE})
message("    Build with Address Sanitizer:              " "BUILD_ASAN              " ${BUILD_ASAN})
message("    Build with udns support:                   " "BUILD_UDNS              " ${BUILD_UDNS})
message("")

if (FORTIFY)
  add_definitions(-D_FORTIFY_SOURCE=2 -O1 -Wl,-z,relro,-z,now)
endif()

# ------------------------------------------------------------------------------
# Resolve dependencies
# ------------------------------------------------------------------------------

# \brief   Find ubuntu packages based on their (optional) include and (required) library test files
# \details
# \param   output_var_prefix    The prefix for variables output_var_prefix{_INCLUDE_DIR,_LIBS} that are set on success
# \param   include_test_file    The file to look for via find_path to see if it exists anywhere
# \param   library_test_file    The file to look for and add to the LIBRARIES variable
# \param   ubuntu_package       The package to add to CPACK package depends list
# \param   ubuntu_build_package The package to suggest installing to proceed building properly
macro(find_ubuntu_package output_var_prefix include_test_file
                library_test_file ubuntu_package
                ubuntu_build_package)

  if (NOT "${include_test_file}" STREQUAL "")
          find_path(${output_var_prefix}_INCLUDE_DIR ${include_test_file})
          if (${output_var_prefix}_INCLUDE_DIR)
                  if(NOT "${output_var_prefix}_INCLUDE_DIR" STREQUAL "/usr/include")
                          message(STATUS "Found ${output_var_prefix} include: ${${output_var_prefix}_INCLUDE_DIR}")
                  endif()
          else()
                  message(FATAL_ERROR "Failed to find ${output_var_prefix} include.  Suggested ubuntu package: ${ubuntu_build_package}")
          endif()
  endif()
  find_library(${output_var_prefix}_LIBS ${library_test_file})
  if (${output_var_prefix}_LIBS)
          message(STATUS "Found ${output_var_prefix} lib: ${${output_var_prefix}_LIBS}")
          LIST(APPEND LIBRARIES ${${output_var_prefix}_LIBS})
  else()
          message(FATAL_ERROR "Failed to find ${output_var_prefix} library.  Suggested ubuntu package: ${ubuntu_build_package}")
  endif()
  if (NOT "${ubuntu_package}" STREQUAL "")
          LIST(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS_LIST "${ubuntu_package}")
  endif()
  if (NOT "${ubuntu_build_package}" STREQUAL "")
          LIST(APPEND CPACK_DEBIAN_PACKAGE_BUILDS_DEPENDS_LIST "${ubuntu_build_package}")
  endif()

endmacro(find_ubuntu_package)

find_ubuntu_package(LIBSSL openssl/ssl.h libssl.a
        libssl1.0.0 libssl-dev)
find_ubuntu_package(LIBCRYPTO openssl/crypto.h libcrypto.a
        libssl1.0.0 libssl-dev)


if (BUILD_PROFILER)
  add_definitions(-DENABLE_PROFILER=1)
  find_ubuntu_package(LIBPROFILER google/profiler.h libprofiler.a
          google-perftools libgoogle-perftools-dev)
endif()

if (BUILD_TCMALLOC)
  find_ubuntu_package(LIBTCMALLOC google/tcmalloc.h libtcmalloc.a
          google-perftools libgoogle-perftools-dev)
  find_ubuntu_package(LIBZMA lzma.h liblzma.a
          liblzma5 liblzma-dev)
  find_ubuntu_package(LIBUNWIND libunwind.h libunwind.a
          libunwind8 libunwind8-dev)
  LIST(APPEND LIBRARIES pthread)  # yes it's wierd as shit to have to include this again...
endif()

if (BUILD_EFENCE)
  find_ubuntu_package(LIBEFENCE "" libefence.a
          "" electric-fence)
endif()

if (BUILD_ASAN)
  find_ubuntu_package(LIBASAN "" libasan.so.0
          "" libasan0)
   add_definitions(-g3 -fno-omit-frame-pointer -fsanitize=address)
   set(DEBUG_MODE ON)
endif()

if(BUILD_UDNS)
  find_ubuntu_package(LIBUDNS udns.h libudns.a
          libudns0 libudns-dev)
endif()

message("   Libraries: ${LIBRARIES}")

# make the cmake list variables into .deb-compatible strings
string(REPLACE ";" ", " CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS_LIST}")
string(REPLACE ";" ", " CPACK_DEBIAN_PACKAGE_BUILDS_DEPENDS "${CPACK_DEBIAN_PACKAGE_BUILDS_DEPENDS_LIST}")


# ------------------------------------------------------------------------------
# Optional flags
# ------------------------------------------------------------------------------
if (DEBUG_MODE)
  add_definitions(-O0 -ggdb)
else()
  add_definitions(-O2)
endif()

# ------------------------------------------------------------------------------
# include source and test directories
# ------------------------------------------------------------------------------
add_subdirectory(src)
add_subdirectory(examples)

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
SET(CPACK_PACKAGE_FILE_NAME "hlx-${CPACK_DEBIAN_PACKAGE_VERSION}-Linux")
SET(CPACK_DEBIAN_PACKAGE_NAME "hlx")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Reed Morrison")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "HTTP Load Tester.")
SET(CPACK_PACKAGE_DESCRIPTION "HTTP Load Tester for Verizon EdgeCast.")
SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
INCLUDE(CPack)
