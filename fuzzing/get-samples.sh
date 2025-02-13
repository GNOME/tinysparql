#!/bin/sh

dir=$1
for f in `find $dir -type f`
do
  echo $f
done
