#!/bin/sh
# generate and import all local .ttl files
# takes as one parameter the number of entries that should be 
# generated in each category

./generate-all.sh $1

for data in *.ttl
do
   tracker-import $data
done
