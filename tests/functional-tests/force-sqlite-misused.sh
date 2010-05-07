#!/bin/bash

if [[ ! -e ../../utils/data-generators/cc/generate ]]; then
    echo "Run this from test/functional-tests/"
    exit 2
fi

# If we don't have ttl files, bring them from the data-generators
if [[ ! -d ttl ]]; then
   CURRENT=`pwd`
   cd ../../utils/data-generators/cc
   # Generation takes time. Don't ask if they are already generated.
   #  Checking just one random .ttl file... not good
   if [[ ! -e ttl/032-nmo_Email.ttl ]]; then
       ./generate max.cfg ;
   else
     echo "TTL directory already exist"
   fi
   cd $CURRENT
   echo "Moving ttl generated files to functional-tests folder"
   cp -R ../../utils/data-generators/cc/ttl . || (echo "Error generating ttl files" && exit 3)
else
   echo "TTL files already in place"
fi

echo "Cleaning tracker DBs and restarting"
tracker-control -rs

echo "Ready, now running the test"
python2.5 10-sqlite-misused.py


