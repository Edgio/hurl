#!/bin/bash
# ==============================================================================
# Run tests with nose
# ==============================================================================
TESTS_LOCATION=$(dirname $0)
if [ -n "$1" ]; then
    nosetests -w ${TESTS_LOCATION} --nocapture --processes=1 --process-timeout=600 $1
else
    nosetests -w ${TESTS_LOCATION} --nocapture --processes=1 --process-timeout=600
fi
