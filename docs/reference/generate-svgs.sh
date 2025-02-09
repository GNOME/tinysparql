#!/bin/bash
command=$1
docs_path=$2

pushd $docs_path >/dev/null

for i in *.dot
do
  filename=`basename -s ".dot" "$i"`
  $command -Tsvg $i >$filename.svg
done

popd >/dev/null
