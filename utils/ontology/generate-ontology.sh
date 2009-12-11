#!/bin/sh

./ontology-graph -d ../../data/ontologies -o ontology.dot
fdp -Tpng -o ontology.png ontology.dot
rm ontology.dot
