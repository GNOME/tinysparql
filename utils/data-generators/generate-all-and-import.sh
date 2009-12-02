#!/bin/sh
# generate and import all local .ttl files
# takes as one parameter the number of entries that should be 
# generated in each category

./generate_all.sh $1
./import_ttl.sh *.ttl
