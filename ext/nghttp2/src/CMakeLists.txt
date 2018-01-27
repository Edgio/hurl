# ------------------------------------------------------------------------------
#   Author: Reed P Morrison
#   Date:   02/20/2016
# ------------------------------------------------------------------------------
# Project setup
project(hurl)
cmake_minimum_required(VERSION 2.8.0)
# ------------------------------------------------------------------------------
# include paths
# ------------------------------------------------------------------------------
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/ext/nghttp2/include
)
# ------------------------------------------------------------------------------
# Source files
# ------------------------------------------------------------------------------
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR} SRCS)
# ------------------------------------------------------------------------------
# Headers
# ------------------------------------------------------------------------------
SET(HDRS
)
# ------------------------------------------------------------------------------
# Build library
# ------------------------------------------------------------------------------
add_library(nghttp2 STATIC ${HDRS} ${SRCS})
target_link_libraries(nghttp2)
# Create symlink for libnghttp2.so.<soversion>
SET_TARGET_PROPERTIES(nghttp2
  PROPERTIES
  SOVERSION 0
)
# Add install target for the library
install(TARGETS nghttp2
  ARCHIVE DESTINATION lib
  COMPONENT Libraries
)
# Add install target for header files
install(FILES ${HDRS}
  DESTINATION include/nghttp2
  COMPONENT Headers
)
