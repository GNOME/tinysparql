#! /usr/bin/python
# -*- coding: utf-8 -*-

import string
import time,datetime
import sys
import ontology_prefixes
import tools

# all ontology modules

 
####################################################################################

# we need a count
if len(sys.argv) != 2:
  print "Usage: %s numberOfItems" % sys.argv[0]
  sys.exit()

try:
  count1 = int(sys.argv[1])
except ValueError:
  print "Invalid item count, not an integer:", sys.argv[1]
  print "Usage: %s numberOfItems" % sys.argv[0]
  sys.exit()

if count1 <= 0:
  print "Invalid item count, must be a positive value:", sys.argv[1]
  print "Usage: %s numberOfItems" % sys.argv[0]
  sys.exit()

print("creating image.ttl file")
f=open('./image.ttl' , 'w')
f.write("@prefix nco:   <http://www.semanticdesktop.org/ontologies/2007/03/22/nco#>.\n")
f.write("@prefix rdfs:  <http://www.w3.org/2000/01/rdf-schema#>.\n")
f.write("@prefix nrl:   <http://www.semanticdesktop.org/ontologies/2007/08/15/nrl#>.\n")
f.write("@prefix nid3:   <http://www.semanticdesktop.org/ontologies/2007/05/10/nid3#>.\n")
f.write("@prefix nmm:   <http://www.tracker-project.org/temp/nmm#>.\n")
f.write("@prefix nao:   <http://www.semanticdesktop.org/ontologies/2007/08/15/nao#>.\n")
f.write("@prefix nfo:   <http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#>.\n")
f.write("@prefix nie:   <http://www.semanticdesktop.org/ontologies/2007/01/19/nie#>.\n");
f.write("@prefix xsd:   <http://www.w3.org/2001/XMLSchema#>.\n\n")
f.write("\n\n")
start_id = int(time.time())
for index in range(start_id, start_id + count1):
   f.write('<urn:photo:'+ `index`+'> a nie:DataObject, nfo:FileDataObject, nfo:Media, nfo:Visual, nfo:Image, nmm:Photo ;\n')
   f.write('\tnie:url               "file:///path/photo'+`index` +'.jpg";\n')
   f.write('\tnie:isStoredAs ' 'urn:uuid:XX:'+` index`+';\n')		
   d1=datetime.datetime.now()
   d2=d1.isoformat()
   f.write('\tnie:contentCreated    "'+d2+'";\n')
   f.write('\tnie:mimeType          "image/jpeg";\n')
   f.write('\tnfo:width             "100";\n')
   f.write('\tnfo:height            "100";\n')
   f.write('\tnfo:fileLastModified  "'+d2+'";\n') 
   #f.write('\tnfo:fileLastAccessed  "'+`(2000 + (index % 10))`+'-'+`( (index % 12) + 1)`+'-'+`( (index % 25) + 1)`+'T01:01:01.'+`index`+'";\n')
   f.write('\tnfo:fileLastAccessed  "'+d2+'";\n')
   f.write('\tnfo:fileSize          "'+`str(1000000 + index)`+'";\n')
   f.write('\tnfo:fileName          "photo'+`index`+'.jpg";\n')
   if ((index-start_id)<(count1/2)):
	 f.write ('\tnao:hasTag [a nao:Tag ; nao:prefLabel  "TEST"];\n')
	 f.write('\tnfo:device            "NOKIA";\n')
   f.write('\tnmm:exposureTime      "'+`index`+'";\n') 
   f.write('\tnmm:fnumber           "'+`(1 + (index % 20))`+'";\n')
   f.write('\tnmm:focalLength       "'+` (1 + (index % 500))`+'".\n')
   f.write('\n\n')
   
f.close()
print("Done")  
