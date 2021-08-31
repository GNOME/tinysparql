#!/bin/sh

cd ${MESON_BUILD_ROOT}/docs/reference/libtracker-sparql/

docs_name=$1
docs_path="${docs_name}-doc/devhelp/books/${docs_name}"
devhelp_file="${docs_path}/*.devhelp2"

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

# Step 3. Trim unnecessary data
rm ${docs_path}/assets/fonts/*.woff*
rm ${docs_path}/assets/fonts/*.svg
rm -rf ${docs_path}/assets/js/search
find ${docs_path} -name "dumped.trie" -delete
