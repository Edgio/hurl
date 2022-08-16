# ------------------------------------------------------------------------------
# Look for required python files
# ------------------------------------------------------------------------------
# TODO: use FindPython (available 3.12) - https://cmake.org/cmake/help/latest/module/FindPython.html
# find_python_module(pytest REQUIRED)
# find_python_module(requests REQUIRED)
# ------------------------------------------------------------------------------
# tests
# ------------------------------------------------------------------------------
#add_test(blackbox_examples_hurl  ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.sh ${CMAKE_CURRENT_SOURCE_DIR}/apps/bb_test_hurl.py)
add_custom_target(blackbox
        ${CMAKE_SOURCE_DIR}/tests/blackbox/run_tests.sh
        DEPENDS blackbox)
