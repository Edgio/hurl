#!/bin/bash
# ------------------------------------------------------------------------------
# To build...
# ------------------------------------------------------------------------------
which cmake g++ make || {
    echo "Failed to find required build packages. Please install with:   sudo apt-get install cmake make g++"
    exit 1
}
mkdir -p build
pushd build && \
    cmake ../ \
    -DBUILD_SYMBOLS=ON \
    -DBUILD_TCMALLOC=ON \
    -DBUILD_TESTS=ON \
    -DBUILD_APPS=ON \
    -DBUILD_LINUX=ON \
    -DCMAKE_INSTALL_PREFIX=/usr && \
    make && \
    umask 0022 && chmod -R a+rX . && \
    make package && \
    make test && \
    popd && \
exit $?
