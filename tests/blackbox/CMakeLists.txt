# ----------------------------------------------------------------------------
# Copyright (C) 2014 Verizon.  All Rights Reserved.
# All Rights Reserved
#
#   Author: Reed P Morrison
#   Date:   01/19/2016
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
# ------------------------------------------------------------------------------
# Resolve dependencies
# ------------------------------------------------------------------------------
# Find if a Python module is installed
# Initial version from: http://www.cmake.org/pipermail/cmake/2011-January/041666.html
# by Mark Mill (mmoll@rice.edu)
# To use do: find_python_module(PyQt4 REQUIRED)
# ------------------------------------------------------------------------------
function(find_python_module module)
        SET(PYTHON_EXECUTABLE python)
        string(TOUPPER ${module} module_upper)
        if(ARGC GREATER 1 AND ARGV1 STREQUAL "REQUIRED")
                set(${module}_FIND_REQUIRED TRUE)
        endif()
        # A module's location is usually a directory, but for binary modules
        # it's a .so file.
        execute_process(COMMAND "${PYTHON_EXECUTABLE}" "-c"
                "import re, ${module}; print(re.compile('/__init__.py.*').sub('',${module}.__file__))"
                RESULT_VARIABLE _${module}_status
                OUTPUT_VARIABLE _${module}_location
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE)
        if("${_${module}_location}" STREQUAL "")
                message(FATAL_ERROR "Failed to find Python module ${module}")
        else()
                message(STATUS "Found Python module ${module} at ${_${module}_location}")
        endif()
endfunction(find_python_module)
# ------------------------------------------------------------------------------
# Look for required python files
# ------------------------------------------------------------------------------
find_python_module(pytest REQUIRED)
find_python_module(requests REQUIRED)
# ------------------------------------------------------------------------------
# tests
# ------------------------------------------------------------------------------
#add_test(blackbox_examples_hurl  ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.sh ${CMAKE_CURRENT_SOURCE_DIR}/apps/bb_test_hurl.py)
add_custom_target(blackbox
        ${CMAKE_SOURCE_DIR}/tests/blackbox/run_tests.sh
        DEPENDS blackbox)
