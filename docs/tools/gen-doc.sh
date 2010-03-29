#!/bin/bash
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

BUILD_DIR="../reference/ontology/"

if ! [ -e $PWD/gen-doc.sh ]; then
	# building documentation out of tree is not supported
	# as documentation is distributed in release tarballs
	exit
fi

echo "Preparing file full text index properties (fts-properties.xml)"

echo "<?xml version='1.0' encoding='UTF-8'?>
<chapter id='fts-properties'>
<title>Full-text indexed properties in the ontology</title>
<table frame='all'>
  <colspec colname='Property'/>
  <colspec colname='Weigth'/>

  <thead>
   <tr>
     <td>Property</td>
     <td>Weigth</td>
   </tr>
  </thead>

<tbody>" > $BUILD_DIR/fts-properties.xml

for f in `find ../../data/ontologies -name "*.description"` ; do
    # ../../data/ontologies/XX-aaa.description -> PREFIX=aaa
    TMPNAME=${f%.description}
    PREFIX=${TMPNAME#*-}
    echo "Generating $PREFIX documentation"

    ./ttl2sgml -d $f -o $BUILD_DIR/$PREFIX-ontology.xml -f $BUILD_DIR/fts-properties.xml \
	-e ../../docs/ontologies/$PREFIX/explanation.xml
done

echo "</tbody></table></chapter>" >> $BUILD_DIR/fts-properties.xml
