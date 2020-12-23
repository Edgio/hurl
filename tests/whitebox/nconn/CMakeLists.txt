# ------------------------------------------------------------------------------
# Project setup
# ------------------------------------------------------------------------------
project(hurl)
cmake_minimum_required(VERSION 2.8.0)
# ------------------------------------------------------------------------------
# wb_nconn_tcp
# ------------------------------------------------------------------------------
SET(SRCS wb_nconn_tcp.cc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/core)
add_executable(wb_nconn_tcp ${HDRS} ${SRCS})
target_link_libraries(wb_nconn_tcp compiled_catch ${CMAKE_BINARY_DIR}/src/core/libhurlcore.a ${LIBRARIES})
SET_TARGET_PROPERTIES(wb_nconn_tcp PROPERTIES OUTPUT_NAME "wb_nconn_tcp")
add_test(nconn_tcp wb_nconn_tcp)
# ------------------------------------------------------------------------------
# wb_nconn_tls
# ------------------------------------------------------------------------------
SET(SRCS wb_nconn_tls.cc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/core)
add_executable(wb_nconn_tls ${HDRS} ${SRCS})
target_link_libraries(wb_nconn_tls compiled_catch ${CMAKE_BINARY_DIR}/src/core/libhurlcore.a ${LIBRARIES})
SET_TARGET_PROPERTIES(wb_nconn_tls PROPERTIES OUTPUT_NAME "wb_nconn_tls")
add_test(nconn_tls wb_nconn_tls)
