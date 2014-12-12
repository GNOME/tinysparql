#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="tracker"

(test -f $srcdir/configure.ac \
  && test -f $srcdir/autogen.sh) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

DIE=0

if ! which gnome-autogen.sh ; then
  echo "You need to install the gnome-common module and make"
  echo "sure the gnome-autogen.sh script is in your \$PATH."
  exit 1
fi

# If no arguments are given, use those used with distcheck
# equally, use the JHBuild prefix if it is available otherwise fall
# back to the default (/usr/local)
if [ $# -eq 0 ] ; then
  echo "Using distcheck arguments, none were supplied..."

  if test -n "$JHBUILD_PREFIX" ; then
    echo "Using JHBuild prefix ('$JHBUILD_PREFIX')"
    NEW_PREFIX="--prefix $JHBUILD_PREFIX"
  fi

  NEW_ARGS="\
	--disable-nautilus-extension \
	--enable-unit-tests \
	--enable-functional-tests \
	--enable-gtk-doc \
	--enable-introspection \
	--disable-miner-rss \
	--disable-miner-evolution \
	--disable-miner-thunderbird \
	--disable-miner-firefox \
	--enable-poppler \
	--enable-exempi \
	--enable-libiptcdata \
	--enable-libjpeg \
	--enable-libtiff \
	--enable-libvorbis \
	--enable-libflac \
	--enable-libgsf \
	--enable-playlist \
	--enable-tracker-preferences \
	--enable-enca"

  set -- $NEW_PREFIX $NEW_ARGS
fi

. gnome-autogen.sh
