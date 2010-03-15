# -*- coding: utf-8 -*-

import string
import random
import datetime

import ontology_prefixes

template_filenames = {}
output_filenames = {}
templates = {}
last_uris = {}
result = {}

####################################################################################
def addType(name, order):
  template = 'template/' + name.replace( '#', '_') + '.tmpl'
  output   = 'ttl/%03d-' % order + name.replace( '#', '_') + '.ttl'

  template_filenames[name] = template
  output_filenames[name] = output
  templates[name] = ''
  result[name] = ''
  last_uris[name] = []

def addResult(type, data):
  result[type] += data + '\n'

def addUri(type, uri):
  last_uris[type].append( uri )

def getLastUri(type):
  return last_uris[type][-1]

def getRandomUri(type):
  return last_uris[type][ int(random.random() * len(last_uris[type]) ) ]

def loadTemplates ():
  for ontology, filename in template_filenames.items():
    content = string.join( open(filename).readlines() )
    templates[ontology] = content

  return len(templates)

def getTemplate(type):
  return templates[type]

def saveResult ():
  for ontology, content in result.items():
    print 'Saving', output_filenames[ontology], '...'
    output = open( output_filenames[ontology], 'w')
    output.write( ontology_prefixes.ontology_prefixes )
    output.write( content )
    output.close()

def getDateNowString():
  return datetime.datetime.today().strftime('%Y-%m-%dT%H:%M:%SZ')

