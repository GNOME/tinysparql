#!/bin/sh
#
# Test runner script for Tracker's functional tests

if test -h /targets/links/scratchbox.config ; then
    export SBOX_REDIRECT_IGNORE=/usr/bin/python ;

    meego-run $@
else
    echo "Running $@"
    $@
fi ;
