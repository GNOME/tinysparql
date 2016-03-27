#!/bin/sh

# Wrap the generated Vala header for libtracker-sparql with a #include guard.
#
# It's important that this only writes the header when necessary; if it gets
# written every time then builds will run more or less from scratch whenever
# Meson needs to reconfigure the project.

set -eu

in="$1"
out="$2"

(echo "#if !defined (__LIBTRACKER_SPARQL_INSIDE__) && !defined (TRACKER_COMPILATION)";
 echo "#error \"only <libtracker-sparql/tracker-sparql.h> must be included directly.\"";
 echo "#endif") > $out
cat $in >> $out
