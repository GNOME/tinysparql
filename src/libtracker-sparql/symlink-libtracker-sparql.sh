#!/bin/sh

libdir=$1
srcdir=$2

case $OSTYPE in
  darwin*) lib_ext="dylib" ;;
  *) lib_ext="so" ;;
esac

for i in `find ${srcdir} -maxdepth 1 -type f,l -name "libtinysparql*${lib_ext}*"`
do
  so_filename=`basename $i`
  libtracker_sparql_so_filename=`echo ${so_filename} | sed s/libtinysparql/libtracker-sparql/`

  rm ${srcdir}/${libtracker_sparql_so_filename}
  ln -s ${so_filename} ${srcdir}/${libtracker_sparql_so_filename}
  cp -a ${srcdir}/${libtracker_sparql_so_filename} ${MESON_INSTALL_DESTDIR_PREFIX}/${libdir}/
done
