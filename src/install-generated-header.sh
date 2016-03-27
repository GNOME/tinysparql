#!/bin/sh

# Script to install generated .h files. This only exists to workaround
# a Meson bug: https://github.com/mesonbuild/meson/issues/705

set -e

HEADER="$1"
TARGET_DIR="$2"

mkdir -p "$DESTDIR$2"
cp $1 "$DESTDIR$2"
