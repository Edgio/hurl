# ------------------------------------------------------------------------------
# Project setup
# ------------------------------------------------------------------------------
project(hurl)
cmake_minimum_required(VERSION 2.8.0)
# ------------------------------------------------------------------------------
# wb_ai_cache
# ------------------------------------------------------------------------------
SET(SRCS wb_ai_cache.cc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/core)
add_executable(wb_ai_cache ${HDRS} ${SRCS})
target_link_libraries(wb_ai_cache
  compiled_catch
  ${CMAKE_BINARY_DIR}/src/core/libhurlcore.a
  ${CMAKE_SOURCE_DIR}/ext/udns-0.4/libudns.a
  ${LIBRARIES}
)
SET_TARGET_PROPERTIES(wb_ai_cache PROPERTIES OUTPUT_NAME "wb_ai_cache")
add_test(ai_cache wb_ai_cache)
# ------------------------------------------------------------------------------
# wb_nresolver
# ------------------------------------------------------------------------------
SET(SRCS wb_nresolver.cc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/core)
add_executable(wb_nresolver ${HDRS} ${SRCS})
target_link_libraries(wb_nresolver
  compiled_catch
  ${CMAKE_BINARY_DIR}/src/core/libhurlcore.a
  ${CMAKE_SOURCE_DIR}/ext/udns-0.4/libudns.a
  ${LIBRARIES}
)
SET_TARGET_PROPERTIES(wb_nresolver PROPERTIES OUTPUT_NAME "wb_nresolver")
add_test(nresolver wb_nresolver)
