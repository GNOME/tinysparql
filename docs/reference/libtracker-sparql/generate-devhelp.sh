#!/bin/bash

docs_name=$1
docs_path=$2
devhelp_file="${docs_name}/${docs_name}.devhelp2"

pushd $docs_path >/dev/null

# Fix .devhelp2 file so it contains keywords from out ontologies
cat $devhelp_file | sed "s/<\/functions>//" - | sed "s/<\/book>//" - >fixed.devhelp2 2>/dev/null
cat ../*-ontology.keywords >>fixed.devhelp2 2>/dev/null
rm ../*-ontology.keywords 2>/dev/null
echo -e "  </functions>\n</book>" >>fixed.devhelp2

# Replace devhelp2 file
mv fixed.devhelp2 $devhelp_file

# Also fix index.json file for web UI search
index_json="${docs_name}/index.json"
python3 ${MESON_SOURCE_ROOT}/docs/reference/libtracker-sparql/merge-json.py $index_json ../*-ontology.index.json
rm ../*-ontology.index.json 2>/dev/null

popd >/dev/null
