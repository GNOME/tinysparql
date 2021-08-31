#!/bin/sh

cd ${MESON_BUILD_ROOT}/docs/reference/libtracker-sparql/

docs_name=$1
echo $docs_name
docs_path="${docs_name}-doc/devhelp/books/${docs_name}"
echo $docs_path
devhelp_file="${docs_path}/*.devhelp2"
echo $devhelp_file

# Step 1. Build devhelp documentation (we let meson do this)
# hotdoc run --conf-file '${docs_name}-doc.json' --devhelp-activate

# Step 2. Fix .devhelp2 file so it contains keywords from out ontologies
cat $devhelp_file | sed "s/<\/functions>//" - | sed "s/<\/book>//" - >fixed.devhelp2

for i in *-ontology.keywords
do
  cat $i >>fixed.devhelp2
done

echo -e "  </functions>\n</book>" >>fixed.devhelp2
mv fixed.devhelp2 $devhelp_file

# Step 3. Install all files
mkdir -p ${MESON_INSTALL_DESTDIR_PREFIX}/share/devhelp/books
cp -a $docs_path ${MESON_INSTALL_DESTDIR_PREFIX}/share/devhelp/books/
