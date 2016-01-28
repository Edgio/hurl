#!/bin/bash
# ==============================================================================
# Run tests with nose
# ==============================================================================
#nosetests -w ./tests/blackbox --with-progressive --nocapture --processes=1 --process-timeout=600
TESTS_LOCATION=$(dirname $0)
nosetests -w ${TESTS_LOCATION} --processes=1 --process-timeout=600
