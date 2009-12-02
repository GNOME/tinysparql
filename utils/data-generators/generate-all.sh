#!/bin/sh
# run all ttl generators
# assumes a generator is prefixed by gen_ and is executable directly

if [ -z $1 ]
then
entries=2000
else
entries=$1
fi

echo "Generating $entries entries per category"

for generator in `ls generate-data-for-*.py`
do
   echo "Running $generator"
   dest=`echo $generator | sed -s "s/\.py/\.ttl/"`

   if test "x$generator" = "xgenerate-data-for-music.py"; then
       args="-T $entries"
   else
       args="$entries"
   fi

   ./$generator $args > $dest
done
