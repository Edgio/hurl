#!/bin/bash
# ==============================================================================
# Run tests with nose
# ==============================================================================
#nosetests -w ./tests/blackbox --with-progressive --nocapture --processes=1 --process-timeout=600
nosetests -w ./tests/blackbox --with-progressive --processes=1 --process-timeout=600
