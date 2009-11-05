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
chmod +x gen_*

for generator in `ls gen_*`
do
  echo "Running $generator"
  ./$generator $entries > ${generator#gen_}.ttl
done

cat contacts_messages.py.ttl
rm contacts_messages.py.ttl
