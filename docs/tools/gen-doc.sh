#!/bin/bash

#
# This script generates the SGML documentation from TTL description
# for the tracker specific ontologies
#
BUILD_DIR="../reference/ontology/"

echo "Generating list of classes-properties and files (file-class.cache)"
if [ -e file-class.cache ]; then
   rm -f file-class.cache ;
fi

for f in `find ../../data/ontologies -name "*.ontology"` ; do
    TMPNAME=${f%.ontology}
    PREFIX=${TMPNAME#*-}
    grep "^[a-z]\{1,\}\:[a-zA-Z]" $f |awk -v pr=$PREFIX '{print pr " " $1}' >> file-class.cache
done

#echo "Converting all dia diagrams to png"
#for image in `find ../../docs/ontologies -name "*.dia"` ; do
#    dia -t png $image -e $BUILD_DIR/$(basename ${image/.dia/.png})
#done

for f in `find ../../data/ontologies -name "*.description"` ; do
    # ../../data/ontologies/XX-aaa.description -> PREFIX=aaa
    TMPNAME=${f%.description}
    PREFIX=${TMPNAME#*-}
    echo "Generating $PREFIX documentation"

    ./ttl2sgml -d $f -o $BUILD_DIR/$PREFIX-ontology.xml -l file-class.cache \
	-e ../../docs/ontologies/$PREFIX/explanation.html
done
