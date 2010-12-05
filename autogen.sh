#!/bin/sh
# Run this to generate all the initial makefiles, etc.
#
# NOTE: compare_versions() is stolen from gnome-autogen.sh

REQUIRED_VALA_VERSION=0.11.2

# Usage:
#     compare_versions MIN_VERSION ACTUAL_VERSION
# returns true if ACTUAL_VERSION >= MIN_VERSION
compare_versions() {
    ch_min_version=$1
    ch_actual_version=$2
    ch_status=0
    IFS="${IFS=         }"; ch_save_IFS="$IFS"; IFS="."
    set $ch_actual_version
    for ch_min in $ch_min_version; do
        ch_cur=`echo $1 | sed 's/[^0-9].*$//'`; shift # remove letter suffixes
        if [ -z "$ch_min" ]; then break; fi
        if [ -z "$ch_cur" ]; then ch_status=1; break; fi
        if [ $ch_cur -gt $ch_min ]; then break; fi
        if [ $ch_cur -lt $ch_min ]; then ch_status=1; break; fi
    done
    IFS="$ch_save_IFS"
    return $ch_status
}

# Vala version check
test -z "$VALAC" && VALAC=valac
VALA_VERSION=`$VALAC --version | cut -d" " -f2`
echo "Found Vala $VALA_VERSION"

if ! compare_versions $REQUIRED_VALA_VERSION $VALA_VERSION; then
    echo "**Error**: You must have valac >= $REQUIRED_VALA_VERSION installed to build, you have $VALA_VERSION"
    exit 1
fi

# Generate required files
test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.
(
  cd "$srcdir" &&
  touch ChangeLog && # Automake requires that ChangeLog exist
  gtkdocize &&
  autopoint --force &&
  AUTOPOINT='intltoolize --automake --copy' autoreconf --verbose --force --install
) || exit
test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"
