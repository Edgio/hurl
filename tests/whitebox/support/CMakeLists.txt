# ------------------------------------------------------------------------------
# wb_nbq
# ------------------------------------------------------------------------------
SET(SRCS wb_nbq.cc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/core)
add_executable(wb_nbq ${HDRS} ${SRCS})
target_link_libraries(wb_nbq compiled_catch  ${CMAKE_BINARY_DIR}/src/core/libhurlcore.a ${LIBRARIES})
SET_TARGET_PROPERTIES(wb_nbq PROPERTIES OUTPUT_NAME "wb_nbq")
add_test(nbq wb_nbq)
# ------------------------------------------------------------------------------
# wb_obj_pool
# ------------------------------------------------------------------------------
SET(SRCS wb_obj_pool.cc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/core)
add_executable(wb_obj_pool ${HDRS} ${SRCS})
target_link_libraries(wb_obj_pool compiled_catch  ${CMAKE_BINARY_DIR}/src/core/libhurlcore.a ${LIBRARIES})
SET_TARGET_PROPERTIES(wb_obj_pool PROPERTIES OUTPUT_NAME "wb_obj_pool")
add_test(obj_pool wb_obj_pool)
# ------------------------------------------------------------------------------
# wb_nlru
# ------------------------------------------------------------------------------
SET(SRCS wb_nlru.cc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/core)
add_executable(wb_nlru ${HDRS} ${SRCS})
target_link_libraries(wb_nlru compiled_catch  ${CMAKE_BINARY_DIR}/src/core/libhurlcore.a ${LIBRARIES})
SET_TARGET_PROPERTIES(wb_nlru PROPERTIES OUTPUT_NAME "wb_nlru")
add_test(nlru wb_nlru)
