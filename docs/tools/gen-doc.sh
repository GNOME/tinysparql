#!/bin/bash

#
# This script generates the SGML documentation from TTL description
# for the tracker specific ontologies
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
