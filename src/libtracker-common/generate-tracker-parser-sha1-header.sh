#!/bin/sh

# Write the Git SHA1 of the libtracker-common subdir to a header file.
#
# This is used in tracker-db-manager.c to regenerate FTS tables if the parser
# code used by the FTS tokenizer code could have changed.
#
# It's important that this script doesn't touch the output file unless it needs
# to. If it updates the file unconditionally, everything will rebuild from
# scratch every time Meson reexecutes.

set -eu

SRCDIR=${MESON_SOURCE_ROOT}/src/libtracker-common
BUILDDIR=${MESON_BUILD_ROOT}/src/libtracker-common

cached_sha1=$(cat ${BUILDDIR}/tracker-parser-sha1.cached || echo "")
new_sha1=$(git -C ${SRCDIR} log -n1 --format=format:%H -- . )

if [ "$cached_sha1" != "$new_sha1" ]; then
	echo "#define TRACKER_PARSER_SHA1 \"${new_sha1}\"" > ${BUILDDIR}/tracker-parser-sha1.h
	echo ${new_sha1} > ${BUILDDIR}/tracker-parser-sha1.cached
fi
