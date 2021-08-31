#!/bin/sh

# Generate fixed .devhelp2 file
${MESON_SOURCE_ROOT}/docs/reference/libtracker-sparql/generate-devhelp.sh $1

# Install all files
docs_name=$1
docs_path="${docs_name}-doc/devhelp/books/${docs_name}"

cd ${MESON_BUILD_ROOT}/docs/reference/libtracker-sparql/
mkdir -p ${MESON_INSTALL_DESTDIR_PREFIX}/share/devhelp/books
cp -a $docs_path ${MESON_INSTALL_DESTDIR_PREFIX}/share/devhelp/books/
