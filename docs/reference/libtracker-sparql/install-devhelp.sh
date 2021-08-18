#!/bin/sh

cd ${MESON_BUILD_ROOT}/docs/reference/libtracker-sparql/

# Step 1. Build devhelp documentation (we let meson do this)
# hotdoc run --conf-file tracker-doc.json --devhelp-activate

# Step 2. Fix .devhelp2 file so it contains keywords from out ontologies
cat *doc/devhelp/books/tracker/*.devhelp2 | sed "s/<\/functions>//" - | sed "s/<\/book>//" - >fixed.devhelp2

for i in *-ontology.keywords
do
  cat $i >>fixed.devhelp2
done

echo -e "  </functions>\n</book>" >>fixed.devhelp2
mv fixed.devhelp2 *doc/devhelp/books/tracker/*.devhelp2

# Step 3. Install all files
mkdir -p ${MESON_INSTALL_PREFIX}/share/devhelp/books
cp -a *doc/devhelp/books/tracker ${MESON_INSTALL_PREFIX}/share/devhelp/books/
