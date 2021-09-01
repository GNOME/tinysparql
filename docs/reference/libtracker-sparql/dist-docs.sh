#!/bin/sh
docs_name=$1

pushd ${MESON_SOURCE_ROOT}
files=`git clean -nx docs/reference/libtracker-sparql/theme-extra/`
if [ -n "$files" ]
then
  echo "Directory 'theme-extra' is unclean:"
  echo -e "$files"
  exit -1
fi
popd

pushd $MESON_BUILD_ROOT

# Ensure the build tree is compiled, we need generated files
ninja

# Run hotdoc manually
pushd docs/reference/libtracker-sparql
hotdoc run --conf-file ${docs_name}-doc.json

# Generate fixed .devhelp2 file
${MESON_SOURCE_ROOT}/docs/reference/libtracker-sparql/generate-devhelp.sh $1

# And copy the resulting devhelp documentation into the dist location
mv ${docs_name}-doc/devhelp/ ${MESON_DIST_ROOT}/docs/reference/libtracker-sparql/

# Delete the remaining docs
rm -rf ${docs_name}-doc/

popd
popd
