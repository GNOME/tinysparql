#!/bin/bash

GLIB_DIR=`pkg-config --variable=prefix glib-2.0`
. ../scripts/xdg_dirs.source

# Ensure we have gtester in PATH
export PATH=$PATH:$GLIB_DIR/bin

make test 2> /dev/null

#./tracker-db-manager-unattach
#./tracker-db-manager-attach
#./tracker-db-manager-custom

. ../scripts/xdg_dirs.unsource
