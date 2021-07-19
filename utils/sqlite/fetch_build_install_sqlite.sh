#!/bin/bash

# Fetch, build SQLite from source and install it into given prefix.
#
# Used for sqlite-compatibility CI job.

set -eu

RELEASE=$1
DOWNLOAD_PATH=$(readlink -f $2)
BUILD_PATH=$(readlink -f $3)
PREFIX=$4

tag=version-${RELEASE}
filename=sqlite-${RELEASE}.tar.gz

(
  cd $DOWNLOAD_PATH
  echo "Download $filename"
  wget "https://www.sqlite.org/src/tarball/sqlite.tar.gz?r=${tag}" --continue -O $filename
)

(
  cd $BUILD_PATH
  echo "Extract $filename"
  tar xf $DOWNLOAD_PATH/$filename

  cd sqlite
  ./configure --prefix=$PREFIX
  make -j
  make install
)
