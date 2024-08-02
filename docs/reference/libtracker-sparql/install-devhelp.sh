#!/bin/sh

docs_name=$1
srcdir=$2
docs_path=$3

# Generate fixed .devhelp2 file
. $srcdir/generate-devhelp.sh $docs_name $docs_path $srcdir

# Install all files
mkdir -p ${MESON_INSTALL_DESTDIR_PREFIX}/share/doc
cp -a $docs_path/* ${MESON_INSTALL_DESTDIR_PREFIX}/share/doc/
