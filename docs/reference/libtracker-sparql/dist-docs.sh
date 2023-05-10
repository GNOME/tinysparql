#!/bin/bash
pushd $MESON_BUILD_ROOT

# Ensure the build tree is compiled, we need generated files
ninja

# Generate fixed .devhelp2 file
. ${MESON_SOURCE_ROOT}/docs/reference/libtracker-sparql/generate-devhelp.sh $1 $2

# And copy the resulting devhelp documentation into the dist location
cp -a $2 ${MESON_DIST_ROOT}/docs/reference/libtracker-sparql/

popd
