#!/bin/sh
# run all ttl generators
# assumes a generator is prefixed by gen_ and is executable directly

if [ -z $1 ]
then
entries=100
else
entries=$1
fi

echo "Generating $entries entries per category"

for generator in `ls generate-data-for-*.py`
do
   echo "Running $generator"
   dest=`echo $generator | sed -s "s/\.py/\.ttl/"`
  ./$generator $entries > $dest
done

#cat contacts.ttl
#rm -f contacts.ttl
