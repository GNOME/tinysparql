#!/bin/sh
#
# Test runner script for Tracker's functional tests

set -e

SCRIPT=$1

DBUS_SESSION_BUS_PID=

export TEMP_DIR=`mktemp --tmpdir -d tracker-test-XXXX`

# We need to use the actual home directory for some tests because
# Tracker will explicitly ignore files in /tmp ...
export REAL_HOME=`echo ~`

# ... but /tmp is preferred for test data, to avoid leaving debris
# in the filesystem
HOME=$TEMP_DIR

if test -h /targets/links/scratchbox.config ; then
    export SBOX_REDIRECT_IGNORE=/usr/bin/python ;

    meego-run $@
else
    eval `dbus-launch --sh-syntax`

    trap "/bin/kill $DBUS_SESSION_BUS_PID; exit" INT

    echo "Running $@"
    $@

    kill $DBUS_SESSION_BUS_PID
fi ;

rm -R $TEMP_DIR
