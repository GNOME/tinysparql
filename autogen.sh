#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="tracker"
REQUIRED_AUTOMAKE_VERSION=1.9

(test -f $srcdir/configure.ac \
  && test -f $srcdir/README) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

test -z "$VALAC" && VALAC=valac

if ! $VALAC --version | sed -e 's/^.*\([0-9]\+\.[0-9]\+\)\.[0-9]\+.*$/\1/' | grep -vq '^0\.[0-6]$'
then
    echo "**Error**: You must have valac >= 0.7.0 installed to build $PKG_NAME"
    exit 1
fi

# Automake requires that ChangeLog exist.
touch ChangeLog

which gnome-autogen.sh || {
    echo "You need to install gnome-common from the GNOME CVS"
    exit 1
}
. gnome-autogen.sh
