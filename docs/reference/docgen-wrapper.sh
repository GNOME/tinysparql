#!/bin/bash

src_dir=`dirname $0`
output_dir=$1; shift
docgen_command=$1; shift

# Build docs
$docgen_command $@

pushd $output_dir >/dev/null
docname=`ls -1 | head -n 1`

# Add ontology keywords to .devhelp2 file
devhelp_file="${docname}/${docname}.devhelp2"
cat $devhelp_file | sed "s/<\/functions>//" - | sed "s/<\/book>//" - >fixed.devhelp2 2>/dev/null
cat ../*-ontology.keywords >>fixed.devhelp2 2>/dev/null
echo -e "  </functions>\n</book>" >>fixed.devhelp2
mv fixed.devhelp2 $devhelp_file

# Also add ontology keywords to index.json for web UI search
index_json="${docname}/index.json"
python3 "${src_dir}/merge-json.py" "${index_json}" ../*-ontology.index.json

popd >/dev/null
