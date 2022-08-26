#!/bin/bash
# ------------------------------------------------------------------------------
# Requirements to build...
# ------------------------------------------------------------------------------
which cmake g++ make || {
    echo "Failed to find required build packages. Please install with: sudo apt-get install cmake make g++"
    exit 1
}
# ------------------------------------------------------------------------------
# Build
# ------------------------------------------------------------------------------
main() {

    build_asan=0
    while getopts ":a" opt; do
        case "${opt}" in
            a)
                build_asan=1
            ;;

            \?)
                echo "Invalid option: -$OPTARG" >&2
                exit $?
            ;;
        esac
    done

    if [ "$(uname)" == "Darwin" ]; then
        NPROC=$(sysctl -n hw.ncpu)
    else
        NPROC=$(nproc)
    fi

    mkdir -p build
    pushd build

    if [[ "${build_asan}" -eq 1 ]]; then
        cmake ../ \
        -DBUILD_SYMBOLS=ON \
        -DDEBUG_MODE=ON \
        -DBUILD_TCMALLOC=OFF \
        -DBUILD_ASAN=ON\
        -DBUILD_TESTS=ON \
        -DBUILD_LINUX=ON \
        -DCMAKE_INSTALL_PREFIX=/usr
    else
        cmake ../ \
        -DBUILD_SYMBOLS=ON \
        -DBUILD_TCMALLOC=ON \
        -DBUILD_TESTS=ON \
        -DBUILD_LINUX=ON \
        -DCMAKE_INSTALL_PREFIX=/usr
    fi

    make -j${NPROC} && \
    make test && \
    umask 0022 && chmod -R a+rX . && \
    make package && \
    popd && \
    exit $?
}

main "${@}"
