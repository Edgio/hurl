# ------------------------------------------------------------------------------
# Project setup
# ------------------------------------------------------------------------------
project(hurl)
cmake_minimum_required(VERSION 2.8.0)
if (POLICY CMP0050)
    cmake_policy(SET CMP0050 OLD)
endif()
# ------------------------------------------------------------------------------
# Build options
# ------------------------------------------------------------------------------
option(GCC_OPTIONS          "Command line options passed to gcc or 'native' to compile for this hardware" OFF)
option(FORTIFY              "Fortify Source GCC options" OFF)
option(DEBUG_MODE           "Compile in debug mode." OFF)
option(BUILD_APPS           "Build hurl/phurl/fsurve" ON)
option(BUILD_TESTS          "Build the unit tests." OFF)
option(BUILD_TCMALLOC       "Build with tcmalloc" OFF)
option(BUILD_PROFILER       "Enable google cpu and heap profiler support" OFF)
option(BUILD_SYMBOLS        "Build with Symbols" OFF)
option(BUILD_ASAN           "Build with Address Sanitizer" OFF)
option(BUILD_UBSAN          "Build with Undefined Behavior Sanitizer" OFF)
option(BUILD_UDNS           "Build with udns (async dns support)" ON)
option(BUILD_LINUX          "Build for Linux" OFF)
option(BUILD_CUSTOM_OPENSSL "Build with custom OpenSSL" OFF)
option(BUILD_KTLS_SUPPORT   "Build with KTLS support - requires OpenSSL to be built with KTLS support as well" OFF)
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
message("    Build with Address Sanitizer:              " "BUILD_ASAN              " ${BUILD_ASAN})
message("    Build with Undefined Behavior Sanitizer:   " "BUILD_UBSAN             " ${BUILD_UBSAN})
message("    Build with udns support:                   " "BUILD_UDNS              " ${BUILD_UDNS})
message("    Build for Linux (adds package help):       " "BUILD_LINUX             " ${BUILD_LINUX})
message("    Build for custom OpenSSL:                  " "BUILD_CUSTOM_OPENSSL    " ${BUILD_CUSTOM_OPENSSL})
message("    Build with KTLS support:                   " "BUILD_KTLS_SUPPORT      " ${BUILD_KTLS_SUPPORT})
message("")
# ------------------------------------------------------------------------------
# fortify options
# ------------------------------------------------------------------------------
if (FORTIFY)
  add_definitions(-D_FORTIFY_SOURCE=2 -O1 -fstack-protector-all -Wl,-z,relro,-z,now)
endif()
# ------------------------------------------------------------------------------
# fail if not found
# ------------------------------------------------------------------------------
macro(fail_if_not_found_library a_lib)
  find_library(${a_lib}_LIBRARY
               NAME ${a_lib}
               PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE})
  if(NOT ${a_lib}_LIBRARY)
    message(FATAL_ERROR "${a_lib} library not found")
  endif()
endmacro(fail_if_not_found_library)
# ------------------------------------------------------------------------------
# ASAN
# ------------------------------------------------------------------------------
if (BUILD_ASAN)
  # TODO not working
  #fail_if_not_found_library(asan)
  set(LIBRARIES ${LIBRARIES} asan)
  add_definitions(-g3 -fno-omit-frame-pointer -fsanitize=address)
  set(DEBUG_MODE ON)
  set(BUILD_PROFILER OFF)
  set(BUILD_TCMALLOC OFF)
# ------------------------------------------------------------------------------
# UBSAN
# ------------------------------------------------------------------------------
elseif (BUILD_UBSAN)
  # TODO not working
  #fail_if_not_found_library(ubsan)
  set(LIBRARIES ${LIBRARIES} ubsan)
  add_definitions(-g3 -fno-omit-frame-pointer -fsanitize=undefined -fno-sanitize-recover)
  set(DEBUG_MODE ON)
  set(BUILD_PROFILER OFF)
  set(BUILD_TCMALLOC OFF)
endif()
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
if(BUILD_LINUX AND BUILD_CUSTOM_OPENSSL)
  INCLUDE_DIRECTORIES("${BUILD_CUSTOM_OPENSSL}/include")
  set(LIBRARIES ${LIBRARIES} "${BUILD_CUSTOM_OPENSSL}/libssl.a")
  set(LIBRARIES ${LIBRARIES} "${BUILD_CUSTOM_OPENSSL}/libcrypto.a")
  fail_if_not_found_library(libz.a)
  set(LIBRARIES ${LIBRARIES} ${libz.a_LIBRARY})
  add_definitions(-DBUILD_CUSTOM_OPENSSL=1)
elseif(BUILD_LINUX)
  fail_if_not_found_library(libssl.a)
  set(LIBRARIES ${LIBRARIES} ${libssl.a_LIBRARY})
  fail_if_not_found_library(libcrypto.a)
  set(LIBRARIES ${LIBRARIES} ${libcrypto.a_LIBRARY})
  fail_if_not_found_library(libz.a)
  set(LIBRARIES ${LIBRARIES} ${libz.a_LIBRARY})
else()
  set(LIBRARIES ${LIBRARIES} pthread ssl crypto)
endif()
# ------------------------------------------------------------------------------
# Libraries options
# ------------------------------------------------------------------------------
set(LIBRARIES ${LIBRARIES} pthread)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(LIBRARIES ${LIBRARIES} rt)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
# ------------------------------------------------------------------------------
# KTLS SUPPORT
# ------------------------------------------------------------------------------
if (BUILD_KTLS_SUPPORT)
    add_definitions(-DKTLS_SUPPORT)
endif()    
# ------------------------------------------------------------------------------
# Build PROFILER
# ------------------------------------------------------------------------------
if (BUILD_PROFILER)
  if (BUILD_LINUX)
    fail_if_not_found_library(libprofiler.a)
    set(LIBRARIES ${LIBRARIES} ${libprofiler.a_LIBRARY})
  else()
    fail_if_not_found_library(profiler)
    set(LIBRARIES ${LIBRARIES} profiler)
  endif()
  add_definitions(-DENABLE_PROFILER=1)
  # profiler requires tcmalloc
  set(BUILD_TCMALLOC ON)
endif()
# ------------------------------------------------------------------------------
# Build TCMALLOC
# ------------------------------------------------------------------------------
if (BUILD_TCMALLOC)
  if (BUILD_LINUX)
    fail_if_not_found_library(libtcmalloc.a)
    fail_if_not_found_library(libunwind.a)
    fail_if_not_found_library(liblzma.a)
    set(LIBRARIES ${LIBRARIES} ${libtcmalloc.a_LIBRARY})
    set(LIBRARIES ${LIBRARIES} ${libunwind.a_LIBRARY})
    set(LIBRARIES ${LIBRARIES} ${liblzma.a_LIBRARY})
    set(LIBRARIES ${LIBRARIES} ${liblzma.a_LIBRARY})
  else()
    fail_if_not_found_library(tcmalloc)
    fail_if_not_found_library(unwind)
    set(LIBRARIES ${LIBRARIES} tcmalloc)
    set(LIBRARIES ${LIBRARIES} unwind)
    set(LIBRARIES ${LIBRARIES} lzma)
  endif()
  set(LIBRARIES ${LIBRARIES} pthread)
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
    BUILD_COMMAND $(MAKE) COMMAND $(MAKE)
    INSTALL_COMMAND ""
  )
  add_definitions(-DASYNC_DNS_WITH_UDNS=1)
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
# Less strict/faster parsing
add_definitions(-DHTTP_PARSER_STRICT=0)
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
if (BUILD_LINUX)
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
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-std=c++17 HAVE_CPP17)
if (HAVE_CPP17)
    add_definitions(-std=c++17)
endif()
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
# ------------------------------------------------------------------------------
# docs
# ------------------------------------------------------------------------------
add_custom_target(docs
    COMMAND doxygen ${CMAKE_CURRENT_SOURCE_DIR}/doxygen.conf WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
)
