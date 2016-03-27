#!/usr/bin/env bash
#
# Generates the SGML documentation from a TTL description
# Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

set -e

if [ $# -lt 5 ]; then
	echo "Insufficient arguments provided"
	echo "Usage: $0 <ttl2sgml> <ttlres2sgml> <ontology-data-dir> <ontology-info-dir> <build-dir>"
	exit 1;
fi

TTL2SGML=$1
TTLRES2SGML=$2
ONTOLOGIES_DATA_DIR=$3
ONTOLOGIES_INFO_DIR=$4
BUILD_DIR=$5

if [ ! -e $BUILD_DIR ]; then
    mkdir -p $BUILD_DIR
fi

$TTLRES2SGML -d $ONTOLOGIES_DATA_DIR -o $BUILD_DIR

for f in `find $ONTOLOGIES_DATA_DIR -name "*.description"` ; do
    # ../../src/ontologies/XX-aaa.description -> PREFIX=aaa
    TMPNAME=${f%.description}
    PREFIX=${TMPNAME#*-}

    $TTL2SGML -d $f -o $BUILD_DIR/$PREFIX-ontology.xml \
	-e $ONTOLOGIES_INFO_DIR/$PREFIX/explanation.xml
done

echo "Done"
