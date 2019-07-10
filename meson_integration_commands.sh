#!/bin/sh

# Post install triggers.
#
# These are needed when following the developer workflow that we suggest in
# README.md, of installing into an isolated prefix like ~/opt/tracker.

set -e

GLIB_COMPILE_SCHEMAS=$1
GSETTINGS_SCHEMAS_DIR=$2

if [ -z "$DESTDIR" ]; then
    $GLIB_COMPILE_SCHEMAS $GSETTINGS_SCHEMAS_DIR
fi
