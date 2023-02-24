#!/bin/sh

# Generate fixed .devhelp2 file
. ${MESON_SOURCE_ROOT}/docs/reference/libtracker-sparql/generate-devhelp.sh $1 $2

# Install all files
docs_path=$2
mkdir -p ${MESON_INSTALL_DESTDIR_PREFIX}/share/doc
cp -a $docs_path/* ${MESON_INSTALL_DESTDIR_PREFIX}/share/doc/
