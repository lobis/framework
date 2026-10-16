#!/usr/bin/env bash
# Build only into a new versioned prefix; never overwrite the active release.
set -eo pipefail
release=${1:?Usage: build_release.sh ABSOLUTE_NEW_RELEASE_DIRECTORY}
case "$release" in /*) ;; *) echo 'Release directory must be absolute' >&2; exit 2;; esac
source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
stack=/data/dust/group/iaxo/software/lobis
if [[ -e "$release/validated.json" || -e "$release/install/validated.json" ]]; then
    echo 'Refusing to rebuild a validated release' >&2
    exit 2
fi
mkdir -p "$release/home/.rest" "$release/system-gtest"
test -f "$release/system-gtest/include/gtest/gtest.h"
test -f "$release/system-gtest/lib/libgtest.so.1.11.0"
test -f "$release/system-gtest/lib/libgtest_main.so.1.11.0"
export HOME="$release/home" REST_HOME="$release/home"
source "$stack/root/root-6.34.04/install/bin/thisroot.sh"
source "$stack/geant4/geant4-v11.0.3/install/bin/geant4.sh"
source "$stack/garfield/garfield-5.0/install/share/Garfield/setupGarfield.sh"
export LD_LIBRARY_PATH="$stack/gsl/gsl-2.7.1/install/lib:$stack/xerces-c/xerces-c-3.3.0/install/lib64:$LD_LIBRARY_PATH"
cat > "$release/system-gtest/CMakeLists.txt" <<'GTEST'
cmake_minimum_required(VERSION 3.16)
project(SystemGTest LANGUAGES CXX)
add_library(gtest_main INTERFACE)
target_include_directories(gtest_main INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(gtest_main INTERFACE ${CMAKE_CURRENT_LIST_DIR}/lib/libgtest_main.so.1.11.0 ${CMAKE_CURRENT_LIST_DIR}/lib/libgtest.so.1.11.0 pthread)
GTEST
cmake -S "$source_dir" -B "$release/build" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$release/install" \
    -DREST_ALL_LIBS=ON -DREST_G4=ON -DREST_GARFIELD=ON -DREST_EVE=ON \
    -DREST_WELCOME=OFF -DTEST=ON \
    -DGSL_ROOT_DIR="$stack/gsl/gsl-2.7.1/install" \
    -DXercesC_ROOT="$stack/xerces-c/xerces-c-3.3.0/install" \
    -DGarfield_DIR="$stack/garfield/garfield-5.0/install/share/Garfield/cmake" \
    -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$release/system-gtest"
cmake --build "$release/build" --parallel "${BUILD_CPUS:-4}"
cmake --install "$release/build"
source "$release/install/thisREST.sh"
ctest --test-dir "$release/build" \
    -R 'FrameworkCore|GDMLParserTest|RunCounterTest|BatchErrors|RestRoot.ExpressionArguments|Setup.RestHome' \
    --output-on-failure
