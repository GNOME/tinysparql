#!/bin/bash
docs_path=$1

# Ensure the build tree is compiled, we need generated files
ninja

# And copy the resulting devhelp documentation into the dist location
cp -a $docs_path ${MESON_DIST_ROOT}/docs/reference/doc/
