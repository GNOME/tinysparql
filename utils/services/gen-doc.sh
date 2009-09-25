#!/bin/bash

#
# This script generates the HTML documentation from TTL description 
# for the tracker specific ontologies
#
BUILD_DIR="./build/ontologies"

if [ -e $BUILD_DIR ]; then
    echo "Removing old " $BUILD_DIR "directory"
    rm -rf $BUILD_DIR
fi

echo "Creating new directory"
mkdir -p $BUILD_DIR

echo "Compiling the tools"
make

echo "Generating list of classes-properties and files"
if [ -e file-class.cache ]; then
   rm -f file-class.cache ;
fi

for f in `find ../../data/ontologies -name "*.ontology"` ; do 
    TMPNAME=${f%.ontology} 
    PREFIX=${TMPNAME#*-}
    grep "^[a-z]\{1,\}\:[a-zA-Z]" $f |awk -v pr=$PREFIX '{print pr " " $1}' >> file-class.cache
done

for f in `find ../../data/ontologies -name "*.description"` ; do 
      # ../../data/ontologies/XX-aaa.description -> PREFIX=aaa
      TMPNAME=${f%.description} 
      PREFIX=${TMPNAME#*-}
      echo "Generating $PREFIX"
      mkdir -p $BUILD_DIR/$PREFIX
      ./ttl2html -d $f -o $BUILD_DIR/$PREFIX/index.html
done

echo "Copying resources"
cp -R resources/ $BUILD_DIR