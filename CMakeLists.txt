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

# Build options
option(GCC_OPTIONS      "Command line options passed to gcc or 'native' to compile for this hardware" OFF)
option(FORTIFY          "Fortify Source GCC options" OFF)
option(DEBUG_MODE       "Compile in debug mode." OFF)
option(BUILD_TESTS      "Build the unit tests." OFF)
option(BUILD_TCMALLOC   "Build with tcmalloc" OFF)
option(BUILD_PROFILER   "Enable google cpu and heap profiler support" OFF)
option(BUILD_SYMBOLS    "Build with Symbols" OFF)
option(BUILD_EFENCE     "Build with tcmalloc" OFF)
option(BUILD_ASAN       "Build with Address Sanitizer" OFF)
option(BUILD_UDNS       "Build with udns (async dns support)" ON)
option(BUILD_UBUNTU     "Build for Ubuntu" OFF)

# ------------------------------------------------------------------------------
# Compiler options
# ------------------------------------------------------------------------------
add_definitions(-std=c++0x)
add_definitions(-Wall -Werror -Weffc++ -Wformat -Wformat-security)
#add_definitions(-pedantic)

# ------------------------------------------------------------------------------
# Libraries options
# ------------------------------------------------------------------------------
set(LIBRARIES ${LIBRARIES} pthread)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(LIBRARIES ${LIBRARIES} rt)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")


# ------------------------------------------------------------------------------
# Display the current settings
# ------------------------------------------------------------------------------
message(STATUS "Build Configuration:")
message("")
message("    Build Option                               Variable                Value                 ")
message("    -----------------------------------------------------------------------------------------")
message("    Debug mode:                                " "DEBUG_MODE              " ${DEBUG_MODE})
message("    Fortify Source:                            " "FORTIFY                 " ${FORTIFY})
message("    Build unit tests:                          " "BUILD_TESTS             " ${BUILD_TESTS})
message("    Install path:                              " "INSTALL_PREFIX          " ${CMAKE_INSTALL_PREFIX})
message("    Enable google cpu/heap profiler support:   " "BUILD_PROFILER          " ${BUILD_PROFILER})
message("    Build Symbols                              " "BUILD_SYMBOLS           " ${BUILD_SYMBOLS})
message("    Build with tcmalloc:                       " "BUILD_TCMALLOC          " ${BUILD_TCMALLOC})
message("    Build with efence:                         " "BUILD_EFENCE            " ${BUILD_EFENCE})
message("    Build with Address Sanitizer:              " "BUILD_ASAN              " ${BUILD_ASAN})
message("    Build with udns support:                   " "BUILD_UDNS              " ${BUILD_UDNS})
message("    Build for Ubuntu (adds package help):      " "BUILD_UBUNTU            " ${BUILD_UBUNTU})
message("")

if (FORTIFY)
  add_definitions(-D_FORTIFY_SOURCE=2 -O1 -Wl,-z,relro,-z,now)
endif()

# ------------------------------------------------------------------------------
# \brief   Find ubuntu packages based on their (optional) include and (required) library test files
# \details
# \param   output_var_prefix    The prefix for variables output_var_prefix{_INCLUDE_DIR,_LIBS} that are set on success
# \param   include_test_file    The file to look for via find_path to see if it exists anywhere
# \param   library_test_file    The file to look for and add to the LIBRARIES variable
# \param   ubuntu_package       The package to add to CPACK package depends list
# \param   ubuntu_build_package The package to suggest installing to proceed building properly
# ------------------------------------------------------------------------------
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
                  message("Failed to find ${output_var_prefix} include.  Suggested ubuntu package: ${ubuntu_build_package}")
                  LIST(APPEND MISSING_UBUNTU_PACKAGES "'${ubuntu_build_package}'")
                  SET(FAILED_PACKAGES 1)
          endif()
  endif()
  if (NOT "${library_test_file}" STREQUAL "")
          find_library(${output_var_prefix}_LIBS ${library_test_file})
          if (${output_var_prefix}_LIBS)
                  message(STATUS "Found ${output_var_prefix} lib: ${${output_var_prefix}_LIBS}")
                  LIST(APPEND LIBRARIES ${${output_var_prefix}_LIBS})
          else()
                  message("Failed to find ${output_var_prefix} library.  Suggested ubuntu package: ${ubuntu_build_package}")
                  LIST(APPEND MISSING_UBUNTU_PACKAGES "'${ubuntu_build_package}'")
                  SET(FAILED_PACKAGES 1)
          endif()
  endif()
  if (NOT "${ubuntu_package}" STREQUAL "")
          LIST(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS_LIST "${ubuntu_package}")
  endif()
  if (NOT "${ubuntu_build_package}" STREQUAL "")
          LIST(APPEND CPACK_DEBIAN_PACKAGE_BUILDS_DEPENDS_LIST "${ubuntu_build_package}")
  endif()
endmacro(find_ubuntu_package)

# ------------------------------------------------------------------------------
# Mac OS X
# ------------------------------------------------------------------------------
IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
   # Mac OS X specific code
   SET(OperatingSystem "Mac OS X")
   # -I/usr/local/opt/openssl/include
   # -L/usr/local/opt/openssl/lib
   #link_directories(/usr/local/opt/openssl/lib)
   # Add MacPorts
   INCLUDE_DIRECTORIES(/usr/local/opt/openssl/include)
   LINK_DIRECTORIES(/usr/local/opt/openssl/lib)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

# ------------------------------------------------------------------------------
# Link OpenSSL
# ------------------------------------------------------------------------------
if (BUILD_UBUNTU)
  find_ubuntu_package(LIBSSL openssl/ssl.h libssl.a libssl1.0.0 libssl-dev)
  find_ubuntu_package(LIBCRYPTO openssl/crypto.h libcrypto.a libssl1.0.0 libssl-dev)
  find_ubuntu_package(LIBZ zlib.h libz.a zlib1g zlib1g-dev)
else()
  set(LIBRARIES ${LIBRARIES} pthread ssl crypto)
endif()

# ------------------------------------------------------------------------------
# Build PROFILER
# ------------------------------------------------------------------------------
if (BUILD_PROFILER)
  add_definitions(-DENABLE_PROFILER=1)
  if (BUILD_UBUNTU)
    find_ubuntu_package(LIBPROFILER google/profiler.h libprofiler.a google-perftools libgoogle-perftools-dev)
  else()
    set(LIBRARIES ${LIBRARIES} tcmalloc profiler)
  endif()
endif()

# ------------------------------------------------------------------------------
# Build TCMALLOC
# ------------------------------------------------------------------------------
if (BUILD_TCMALLOC)
  if (BUILD_UBUNTU)
    find_ubuntu_package(LIBTCMALLOC google/tcmalloc.h libtcmalloc.a "" "")
    find_ubuntu_package(LIBZMA lzma.h liblzma.a liblzma5 liblzma-dev)
    find_ubuntu_package(LIBUNWIND libunwind.h libunwind.a "" "")
    LIST(APPEND LIBRARIES pthread)  # yes it's wierd as shit to have to include this again...
  else()
    set(LIBRARIES ${LIBRARIES} tcmalloc)
  endif()
endif()

# ------------------------------------------------------------------------------
# Build EFENCE
# ------------------------------------------------------------------------------
if (BUILD_EFENCE)
  if (BUILD_UBUNTU)
    find_ubuntu_package(LIBEFENCE "" libefence.a "" electric-fence)
  else()
    set(LIBRARIES ${LIBRARIES} efence)
  endif()
endif()

# ------------------------------------------------------------------------------
# ASAN
# ------------------------------------------------------------------------------
if (BUILD_ASAN)
  if (BUILD_UBUNTU)
    find_ubuntu_package(LIBASAN "" libasan.so.0 "" libasan0)
  else()
    set(LIBRARIES ${LIBRARIES} asan)
  endif()
  add_definitions(-g3 -fno-omit-frame-pointer -fsanitize=address)
  set(DEBUG_MODE ON)
endif()

# ------------------------------------------------------------------------------
# Build UDNS
# ------------------------------------------------------------------------------
if(BUILD_UDNS)
  include(ExternalProject)
  ExternalProject_Add(ext_udns
    # Optional approach -including from external url
    #URL http://www.corpit.ru/mjt/udns/udns-0.4.tar.gz
    DOWNLOAD_COMMAND ""
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/ext/udns-0.4
    BINARY_DIR ${CMAKE_SOURCE_DIR}/ext/udns-0.4
    CONFIGURE_COMMAND "./configure"
    BUILD_COMMAND make
    INSTALL_COMMAND ""
  )  
  add_definitions(-DASYNC_DNS_WITH_UDNS=1)
endif()

if (FAILED_PACKAGES)
  LIST(REMOVE_DUPLICATES MISSING_UBUNTU_PACKAGES)
  string(REPLACE ";" " " MISSING_UBUNTU_PACKAGES_STR "${MISSING_UBUNTU_PACKAGES}")
  message(FATAL_ERROR "  Please install suggested packages:   sudo apt-get install ${MISSING_UBUNTU_PACKAGES_STR}   and try again.")
endif()

# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
set(LIBRARIES ${LIBRARIES} dl)
message(STATUS "Libraries: ${LIBRARIES}")

# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
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
if (BUILD_SYMBOLS)
  add_definitions(-ggdb)
endif()

# ------------------------------------------------------------------------------
# Version
# ------------------------------------------------------------------------------
EXECUTE_PROCESS(COMMAND git describe --tags OUTPUT_VARIABLE VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT ${VERSION} MATCHES ^[0-9]+.[0-9]+)
        message(FATAL_ERROR "Most recent tag '${VERSION}' does not match required version regex: ^[0-9]+.[0-9]+
Does it start with a number?")
endif()
add_definitions(-DHLX_VERSION="${VERSION}")

# ------------------------------------------------------------------------------
# Debian Package Support
# ------------------------------------------------------------------------------
if (BUILD_UBUNTU)
  EXECUTE_PROCESS(COMMAND lsb_release -cs OUTPUT_VARIABLE DISTRIBUTION OUTPUT_STRIP_TRAILING_WHITESPACE)
  SET(CPACK_GENERATOR "DEB")
  SET(CPACK_DEBIAN_PACKAGE_VERSION "${VERSION}-${DISTRIBUTION}")
  SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
  SET(CPACK_PACKAGE_FILE_NAME "hlx_${CPACK_DEBIAN_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
  SET(CPACK_DEBIAN_PACKAGE_NAME "hlx")
  SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Reed Morrison")
  SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "HTTP Load Tester.")
  SET(CPACK_PACKAGE_DESCRIPTION "HTTP Load Tester for Verizon EdgeCast.")
  message(STATUS "Package Configuration:")
  message("")
  message("    Option                                     Value                     ")
  message("    ---------------------------------------------------------------------")
  message("    Package Version:                           ${CPACK_DEBIAN_PACKAGE_VERSION}")
  message("")

  INCLUDE(CPack)

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
