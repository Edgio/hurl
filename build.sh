#!/bin/bash
# ----------------------------------------------------------------------------
# Copyright (C) 2014 Verizon.  All Rights Reserved.
# All Rights Reserved
#
#   Author: Reed P Morrison
#   Date:   02/07/2014  
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

# ------------------------------------------------------------------------------
# Requirements:
#   libssl-dev
#   libgoogle-perftools-dev
#   libmicrohttpd-dev
# ------------------------------------------------------------------------------
BUILD_DEPENDS="$(grep BUILDS_DEPENDS CMakeLists.txt | sed -e 's/.*"\(.*\)".*/\1/')"
echo "Checking build dependencies (manually):"

while read pkg; do
    echo -n "  $pkg..."

    pkg_name="${pkg%% *}"
    pkg_ver="${pkg##* }"
    pkg_ver="$(echo "${pkg_ver}" | sed -e 's/.*([<>=]*\([0-9].*\))/\1/')"
    pkg_op="$(echo "${pkg_ver}" | sed -e 's/^\([^0-9]*\).*/\1/')"

    if [[ "${pkg_ver}" == "${pkg_name}" ]]; then pkg_ver="0.0.0"; fi

    installed_ver="$(dpkg-query --show --showformat '${Version}' $pkg_name)"
    if [[ $? == 0 ]] &&
        dpkg --compare-versions "${pkg_ver}" le "${installed_ver}"; then
        echo "  pass"
    else
        echo "  fail.  Please install this package: ${pkg_name} with version ${pkg_ver}"
        exit 1
    fi
done <<<"$(echo "${BUILD_DEPENDS}" | tr ',' '\n')"

# ------------------------------------------------------------------------------
# To build...
# ------------------------------------------------------------------------------
mkdir -p build
pushd build
cmake ../
make package
popd
