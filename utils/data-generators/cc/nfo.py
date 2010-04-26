# -*- coding: utf-8 -*-

import tools
import gen_data as gen

####################################################################################
nfo_PlainTextDocument = '''
<%(plaintext_document_uri)s> a nfo:PlainTextDocument, nfo:FileDataObject;
    nie:byteSize           "%(plaintext_document_size)s" ;
    nie:dataSource         <%(plaintext_document_datasource)s> ;
    nie:url                "%(plaintext_document_url)s" ;
    nfo:belongsToContainer <%(plaintext_document_container)s> ;
    nie:mimeType           "%(plaintext_document_mime)s" ;
    nie:plainTextContent   "%(plaintext_document_content)s" ;
    nie:isStoredAs         <%(plaintext_document_stored_as)s> ;
    nfo:fileLastAccessed   "%(plaintext_document_last_accessed)s" ;
    nfo:fileLastModified   "%(plaintext_document_last_modified)s" ;
    nfo:fileSize           "%(plaintext_document_size)s" ;
    nfo:fileName           "%(plaintext_document_filename)s" .
'''
def generatePlainTextDocument(index):
  me = 'nfo#PlainTextDocument'
  plaintext_document_datasource    = tools.getRandomUri( 'tracker#Volume' )
  plaintext_document_filename      = 'Plain_text_document_%d.txt' % (index % 1000)
  plaintext_document_container     = 'file://documents/'
  plaintext_document_url           = plaintext_document_container + plaintext_document_filename
  plaintext_document_uri           = plaintext_document_url
  plaintext_document_mime          = 'text/plain'
  plaintext_document_content       = gen.create_text(5,300)
  plaintext_document_stored_as     = plaintext_document_url
  plaintext_document_last_accessed ='%d-%02d-%02dT01:01:01Z' % (2000 + (index % 10), (index % 12) + 1, (index % 25) + 1)
  plaintext_document_last_modified ='%d-%02d-%02dT01:01:01Z' % (2000 + (index % 10), (index % 12) + 1, (index % 25) + 1)
  plaintext_document_size          = '%d' % (1 + (index % 1000000))

  tools.addItem( me, plaintext_document_uri, nfo_PlainTextDocument % locals() )

####################################################################################
nfo_SoftwareCategory = '''
<%(application_category_uri)s> a nfo:SoftwareCategory;
  nie:title "%(application_category_title)s" .
'''
def generateSoftwareCategory(index):
  me = 'nfo#SoftwareCategory'
  application_category_uri   = 'urn:software-category:%d' % index
  application_category_title = 'Category %d' % (index % 1000)

  tools.addItem( me, application_category_uri, nfo_SoftwareCategory % locals() )

####################################################################################
nfo_SoftwareApplication = '''
<%(application_uri)s> a nfo:SoftwareApplication, nfo:FileDataObject;
    nie:title            "%(application_title)s";
    nie:url              "%(application_url)s";
    nie:dataSource       <%(application_datasource)s>;
    nie:isStoredAs       <%(application_url)s>;
    nie:isLogicalPartOf  <%(application_part_of)s>;
    nfo:fileName         "%(application_filename)s";
    nfo:fileLastModified "%(application_last_modified)s";
    nfo:softwareIcon     <%(application_icon)s>;
    nfo:softwareCmdLine  "%(application_cmdline)s" .
'''
def generateSoftwareApplication(index):
  me = 'nfo#SoftwareApplication'
  application_cmdline       = 'app%d' % index
  application_filename      = application_cmdline + '.desktop'
  application_uri           = 'file://applications/' + application_filename
  application_title         = 'Application %d' % index
  application_part_of       = tools.getRandomUri( 'nfo#SoftwareCategory' )
  application_url           = application_uri
  application_datasource    = tools.getRandomUri( 'tracker#Volume' )
  application_last_modified = '%d-%02d-%02dT01:01:01Z' % (2000 + (index % 10), (index % 12) + 1, (index % 25) + 1)
  application_icon          = 'urn:theme-icon:Icon-%d' % index

  tools.addItem( me, application_uri, nfo_SoftwareApplication % locals() )

####################################################################################
nfo_WebHistory = '''
<%(webhistory_uri)s> a nfo:WebHistory ;
  nie:title                   "%(webhistory_title)s" ;
  nie:informationElementDate  "%(webhistory_date)s" ;
  nie:contentCreated          "%(webhistory_created)s" ;
  nie:contentLastModified     "%(webhistory_modified)s" ;
  nie:usageCounter            "%(webhistory_counter)s" ;
  nfo:uri                     "%(webhistory_url)s" .
'''
def generateWebHistory(index):
  me = 'nfo#WebHistory'
  webhistory_uri       = 'urn:webhistory:%d' % index
  webhistory_title     = 'Web history %d' % (index % 1000)
  webhistory_date      = tools.now
  webhistory_created   = webhistory_date
  webhistory_modified  = webhistory_date
  webhistory_counter   = '%d' % (index % 10)
  webhistory_url       = 'http://www.%d.com/' % index

  tools.addItem( me, webhistory_uri, nfo_WebHistory % locals() )

