#!/bin/bash
docs_name=$1
srcdir=$2
docs_path=$3

# Ensure the build tree is compiled, we need generated files
ninja

# Generate fixed .devhelp2 file
. $srcdir/generate-devhelp.sh $docs_name $docs_path $srcdir

# And copy the resulting devhelp documentation into the dist location
cp -a $docs_path ${MESON_DIST_ROOT}/docs/reference/libtracker-sparql/
