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
project(hurl)
cmake_minimum_required(VERSION 2.8.0)

if (POLICY CMP0050)
    cmake_policy(SET CMP0050 OLD)
endif()

# Build options
option(GCC_OPTIONS       "Command line options passed to gcc or 'native' to compile for this hardware" OFF)
option(FORTIFY           "Fortify Source GCC options" OFF)
option(DEBUG_MODE        "Compile in debug mode." OFF)
option(BUILD_APPS        "Build hurl/phurl/fsurve" ON)
option(BUILD_TESTS       "Build the unit tests." OFF)
option(BUILD_TCMALLOC    "Build with tcmalloc" OFF)
option(BUILD_PROFILER    "Enable google cpu and heap profiler support" OFF)
option(BUILD_SYMBOLS     "Build with Symbols" OFF)
option(BUILD_EFENCE      "Build with efence" OFF)
option(BUILD_ASAN        "Build with Address Sanitizer" OFF)
option(BUILD_UBSAN       "Build with Undefined Behavior Sanitizer" OFF)
option(BUILD_UDNS        "Build with udns (async dns support)" ON)
option(BUILD_UBUNTU      "Build for Ubuntu" OFF)
# ------------------------------------------------------------------------------
# Display the current settings
# ------------------------------------------------------------------------------
message(STATUS "Build Configuration:")
message("")
message("    Build Option                               Variable                Value                 ")
message("    -----------------------------------------------------------------------------------------")
message("    Debug mode:                                " "DEBUG_MODE              " ${DEBUG_MODE})
message("    Fortify Source:                            " "FORTIFY                 " ${FORTIFY})
message("    Build applications (hurl/phurl/fsurv):     " "BUILD_APPS              " ${BUILD_APPS})
message("    Build unit tests:                          " "BUILD_TESTS             " ${BUILD_TESTS})
message("    Install path:                              " "INSTALL_PREFIX          " ${CMAKE_INSTALL_PREFIX})
message("    Enable google cpu/heap profiler support:   " "BUILD_PROFILER          " ${BUILD_PROFILER})
message("    Build Symbols                              " "BUILD_SYMBOLS           " ${BUILD_SYMBOLS})
message("    Build with tcmalloc:                       " "BUILD_TCMALLOC          " ${BUILD_TCMALLOC})
message("    Build with efence:                         " "BUILD_EFENCE            " ${BUILD_EFENCE})
message("    Build with Address Sanitizer:              " "BUILD_ASAN              " ${BUILD_ASAN})
message("    Build with Undefined Behavior Sanitizer:   " "BUILD_UBSAN             " ${BUILD_UBSAN})
message("    Build with udns support:                   " "BUILD_UDNS              " ${BUILD_UDNS})
message("    Build for Ubuntu (adds package help):      " "BUILD_UBUNTU            " ${BUILD_UBUNTU})
message("")

if (FORTIFY)
  add_definitions(-D_FORTIFY_SOURCE=2 -O1 -fstack-protector-all -Wl,-z,relro,-z,now)
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

  # handle optional prefix_path arg
  if (${ARGC} EQUAL 6)
          set(prefix_path "${ARGN}")
          message(STATUS "Got prefix path: '${prefix_path}' for package: ${ubuntu_build_package}")
          # set(list_var "${ARGN}")
          # message(STATUS "got ARGC: ${ARGC}")
          # foreach(loop_var IN LISTS list_var)
          #         message(STATUS "got arg: ${loop_var}")
          # endforeach()
  endif()

  if (NOT "${include_test_file}" STREQUAL "")
          find_path(${output_var_prefix}_INCLUDE_DIR ${include_test_file}) # HINTS ${prefix_path} NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEN_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)
          if (${output_var_prefix}_INCLUDE_DIR)
                  #if(NOT "${output_var_prefix}_INCLUDE_DIR" STREQUAL "/usr/include")
                          message(STATUS "Found ${output_var_prefix} include: ${${output_var_prefix}_INCLUDE_DIR}")
                  #endif()
          else()
                  message("aaaa Failed to find ${output_var_prefix} include.  Suggested ubuntu package: ${ubuntu_build_package}")
                  LIST(APPEND MISSING_UBUNTU_PACKAGES "'${ubuntu_build_package}'")
                  SET(FAILED_PACKAGES 1)
          endif()
  endif()
  if (NOT "${library_test_file}" STREQUAL "")
          find_library(${output_var_prefix}_LIBS ${library_test_file} HINTS ${prefix_path} NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH)
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
# ASAN
# ------------------------------------------------------------------------------
if (BUILD_ASAN)
  set(LIBRARIES ${LIBRARIES} asan)
  add_definitions(-g3 -fno-omit-frame-pointer -fsanitize=address)
  set(DEBUG_MODE ON)
  set(BUILD_PROFILER OFF)
  set(BUILD_TCMALLOC OFF)
# ------------------------------------------------------------------------------
# UBSAN
# ------------------------------------------------------------------------------
elseif (BUILD_UBSAN)
  set(LIBRARIES ${LIBRARIES} ubsan)
  add_definitions(-g3 -fno-omit-frame-pointer -fsanitize=undefined -fno-sanitize-recover)
  set(DEBUG_MODE ON)
  set(BUILD_PROFILER OFF)
  set(BUILD_TCMALLOC OFF)
endif()

# ------------------------------------------------------------------------------
# Libraries options
# ------------------------------------------------------------------------------
set(LIBRARIES ${LIBRARIES} pthread)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(LIBRARIES ${LIBRARIES} rt)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

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
    find_ubuntu_package(LIBTCMALLOC google/tcmalloc.h libtcmalloc.a "" libgoogle-perftools-dev)
    find_ubuntu_package(LIBZMA lzma.h liblzma.a liblzma5 liblzma-dev)
    find_ubuntu_package(LIBUNWIND libunwind.h libunwind.a "" libunwind8-dev)
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
  add_definitions(-O0 -g3)
else()
  add_definitions(-O2)
endif()
if (BUILD_SYMBOLS)
  add_definitions(-g3)
endif()

# ------------------------------------------------------------------------------
# Version
# ------------------------------------------------------------------------------
EXECUTE_PROCESS(COMMAND git describe --tags OUTPUT_VARIABLE VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT ${VERSION} MATCHES ^[0-9]+.[0-9]+)
        message(FATAL_ERROR "Most recent tag '${VERSION}' does not match required version regex: ^[0-9]+.[0-9]+
Does it start with a number?")
endif()
add_definitions(-DHURL_VERSION="${VERSION}")

# ------------------------------------------------------------------------------
# Debian Package Support
# ------------------------------------------------------------------------------
if (BUILD_UBUNTU)
  EXECUTE_PROCESS(COMMAND lsb_release -cs OUTPUT_VARIABLE DISTRIBUTION OUTPUT_STRIP_TRAILING_WHITESPACE)
  SET(CPACK_GENERATOR "DEB")
  SET(CPACK_DEBIAN_PACKAGE_VERSION "${VERSION}-${DISTRIBUTION}")
  SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
  SET(CPACK_PACKAGE_FILE_NAME "hurl_${CPACK_DEBIAN_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
  SET(CPACK_DEBIAN_PACKAGE_NAME "hurl")
  SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Reed Morrison")
  SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "HTTP Server Utils.")
  SET(CPACK_PACKAGE_DESCRIPTION "HTTP Server Utils for EdgeCast Networks.")
  message(STATUS "Package Configuration:")
  message("")
  message("    Option                                     Value                     ")
  message("    ---------------------------------------------------------------------")
  message("    Package Version:                           ${CPACK_DEBIAN_PACKAGE_VERSION}")
  message("")
  INCLUDE(CPack)
endif()
# ------------------------------------------------------------------------------
# build nghttp2
# ------------------------------------------------------------------------------
add_subdirectory(ext/nghttp2/src)
# ------------------------------------------------------------------------------
# Compiler options
# ------------------------------------------------------------------------------
add_definitions(-std=c++0x)
add_definitions(-Wall -Werror -Weffc++ -Wformat -Wformat-security)
#add_definitions(-pedantic)
# ------------------------------------------------------------------------------
# include source and test directories
# ------------------------------------------------------------------------------
add_subdirectory(src)
if (BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ------------------------------------------------------------------------------
# add uninstall target
# ------------------------------------------------------------------------------
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)

add_custom_target(uninstall
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
