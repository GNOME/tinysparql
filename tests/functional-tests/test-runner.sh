#!/bin/sh
#
# Test runner script for Tracker's functional tests

SCRIPT=$1
if test "$SCRIPT" = "python" ; then SCRIPT=$2 ; fi
if ! test -x $SCRIPT ; then
    echo "(Skipping $@ - not executable)"
    exit
fi

DBUS_SESSION_BUS_PID=

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
