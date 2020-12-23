#!/bin/bash
# ------------------------------------------------------------------------------
# To build...
# ------------------------------------------------------------------------------
which cmake || {
    echo "Failed to find all required apps to build (cmake)."
    exit 1
}
mkdir -p build
pushd build && \
    cmake ../
    make
    popd && \
exit $?
