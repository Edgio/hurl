# ------------------------------------------------------------------------------
# Project setup
# ------------------------------------------------------------------------------
project(hurl)
cmake_minimum_required(VERSION 3.10)
# ------------------------------------------------------------------------------
# Build options
# ------------------------------------------------------------------------------
option(GCC_OPTIONS          "Command line options passed to gcc or 'native' to compile for this hardware" OFF)
option(FORTIFY              "Fortify Source GCC options" OFF)
option(DEBUG_MODE           "Compile in debug mode." OFF)
option(BUILD_SYMBOLS        "Build with Symbols" OFF)
option(BUILD_TCMALLOC       "Build with tcmalloc" OFF)
option(BUILD_PROFILER       "Enable google cpu and heap profiler support" OFF)
option(BUILD_ASAN           "Build with Address Sanitizer" OFF)
option(BUILD_UBSAN          "Build with Undefined Behavior Sanitizer" OFF)
option(BUILD_CUSTOM_OPENSSL "Build with custom OpenSSL" OFF)
option(BUILD_TESTS          "Build the unit tests." OFF)
option(BUILD_LINUX          "Build for Linux" OFF)
option(BUILD_KTLS_SUPPORT   "Build with KTLS support - requires OpenSSL to be built with KTLS support as well" OFF)
# ------------------------------------------------------------------------------
# Display the current settings
# ------------------------------------------------------------------------------
message(STATUS "Build Configuration:")
message("")
message("    Build Option                               Variable                Value                 ")
message("    -----------------------------------------------------------------------------------------")
message("    Install path:                              " "INSTALL_PREFIX          " ${CMAKE_INSTALL_PREFIX})
message("    Fortify Source:                            " "FORTIFY                 " ${FORTIFY})
message("    Debug mode:                                " "DEBUG_MODE              " ${DEBUG_MODE})
message("    Build Symbols                              " "BUILD_SYMBOLS           " ${BUILD_SYMBOLS})
message("    Build with tcmalloc:                       " "BUILD_TCMALLOC          " ${BUILD_TCMALLOC})
message("    Enable google cpu/heap profiler support:   " "BUILD_PROFILER          " ${BUILD_PROFILER})
message("    Build with Address Sanitizer:              " "BUILD_ASAN              " ${BUILD_ASAN})
message("    Build with Undefined Behavior Sanitizer:   " "BUILD_UBSAN             " ${BUILD_UBSAN})
message("    Build for custom OpenSSL:                  " "BUILD_CUSTOM_OPENSSL    " ${BUILD_CUSTOM_OPENSSL})
message("    Build unit tests:                          " "BUILD_TESTS             " ${BUILD_TESTS})
message("    Build for Linux (adds package help):       " "BUILD_LINUX             " ${BUILD_LINUX})
message("    Build with KTLS support:                   " "BUILD_KTLS_SUPPORT      " ${BUILD_KTLS_SUPPORT})
message("")
# ------------------------------------------------------------------------------
# OS X specific code
# ------------------------------------------------------------------------------
IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
   SET(OperatingSystem "Mac OS X")
   INCLUDE_DIRECTORIES(/usr/local/opt/rapidjson/include)
   # Add MacPorts
   if(BUILD_TLS)
     INCLUDE_DIRECTORIES(/usr/local/opt/openssl/include)
     LINK_DIRECTORIES(/usr/local/opt/openssl/lib)
   endif()
   if(BUILD_TCMALLOC OR BUILD_PROFILER)
     INCLUDE_DIRECTORIES(/usr/local/opt/gperftools/include)
     LINK_DIRECTORIES(/usr/local/opt/gperftools/lib)
   endif()
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
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
if(BUILD_ASAN)
  set(DEBUG_LIBRARIES asan ${DEBUG_LIBRARIES})
  add_definitions(-fno-omit-frame-pointer -fsanitize=address)
  set(DEBUG_MODE ON)
  set(BUILD_PROFILER OFF)
  set(BUILD_TCMALLOC OFF)
# ------------------------------------------------------------------------------
# UBSAN
# ------------------------------------------------------------------------------
elseif(BUILD_UBSAN)
  set(DEBUG_LIBRARIES asan ${DEBUG_LIBRARIES})
  add_definitions(-fno-omit-frame-pointer -fsanitize=undefined -fno-sanitize-recover)
  set(DEBUG_MODE ON)
  set(BUILD_PROFILER OFF)
  set(BUILD_TCMALLOC OFF)
endif()
# ------------------------------------------------------------------------------
# Optional flags
# ------------------------------------------------------------------------------
if (DEBUG_MODE)
  add_definitions(-O0 -g3)
elseif (BUILD_SYMBOLS)
  add_definitions(-g3)
else()
  add_definitions(-O2)
endif()
# Less strict/faster parsing
add_definitions(-DHTTP_PARSER_STRICT=0)
# ------------------------------------------------------------------------------
# add ssl libraries
# ------------------------------------------------------------------------------
set(LIBRARIES crypto ssl pthread)
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
  elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    fail_if_not_found_library(tcmalloc)
    set(LIBRARIES ${LIBRARIES} tcmalloc)
    set(LIBRARIES ${LIBRARIES} lzma)
  else()
    fail_if_not_found_library(tcmalloc)
    fail_if_not_found_library(unwind)
    set(LIBRARIES ${LIBRARIES} tcmalloc)
    set(LIBRARIES ${LIBRARIES} unwind)
    set(LIBRARIES ${LIBRARIES} lzma)
  endif()
endif()
# ------------------------------------------------------------------------------
# special build case for OPENSSL
# ------------------------------------------------------------------------------
if(BUILD_CUSTOM_OPENSSL)
  INCLUDE_DIRECTORIES("${BUILD_CUSTOM_OPENSSL}/include")
  LINK_DIRECTORIES("${BUILD_CUSTOM_OPENSSL}")
  LINK_DIRECTORIES("${BUILD_CUSTOM_OPENSSL}/lib")
endif()
# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------
# make the cmake list variables into .deb-compatible strings
set(CPACK_DEBIAN_PACKAGE_DEPENDS_LIST
	"libssl-dev"
)
string(REPLACE ";" ", " CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS_LIST}")
set(CPACK_DEBIAN_PACKAGE_BUILDS_DEPENDS_LIST
	"libssl-dev"
)
string(REPLACE ";" ", " CPACK_DEBIAN_PACKAGE_BUILDS_DEPENDS "${CPACK_DEBIAN_PACKAGE_BUILDS_DEPENDS_LIST}")
# ------------------------------------------------------------------------------
# Version
# ------------------------------------------------------------------------------
EXECUTE_PROCESS(COMMAND git describe --tags OUTPUT_VARIABLE VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
add_definitions(-DHURL_VERSION="${VERSION}")
# ------------------------------------------------------------------------------
# Debian Package Support
# ------------------------------------------------------------------------------
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  SET(DISTRIBUTION "macOS")
else()
  EXECUTE_PROCESS(COMMAND lsb_release -cs OUTPUT_VARIABLE DISTRIBUTION OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
SET(CPACK_GENERATOR "DEB")
SET(CPACK_DEBIAN_PACKAGE_VERSION "${VERSION}-${DISTRIBUTION}")
SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
SET(CPACK_PACKAGE_FILE_NAME "hurl_${CPACK_DEBIAN_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
SET(CPACK_DEBIAN_PACKAGE_NAME "hurl")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Reed Morrison")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "HTTP Server Utils.")
SET(CPACK_PACKAGE_DESCRIPTION "HTTP Server Utils for Edgio Inc.")
message(STATUS "Package Configuration:")
message("")
message("    Option                                     Value                     ")
message("    ---------------------------------------------------------------------")
message("    Package Version:                           ${CPACK_DEBIAN_PACKAGE_VERSION}")
message("")
INCLUDE(CPack)
# ------------------------------------------------------------------------------
# include source and test directories
# ------------------------------------------------------------------------------
add_subdirectory(ext/nghttp2/src)
add_subdirectory(src)
if(BUILD_TESTS)
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
