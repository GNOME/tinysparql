# -*- coding: utf-8 -*-

import tools
import gen_data as gen

####################################################################################
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

  # save the last uri
  tools.addUri( me, plaintext_document_uri )

  # subsitute into template
  doc = tools.getTemplate( me )

  # save the result
  tools.addResult( me, doc % locals() )

####################################################################################
def generateSoftwareCategory(index):
  me = 'nfo#SoftwareCategory'
  application_category_uri   = 'urn:software-category:%d' % index
  application_category_title = 'Category %d' % (index % 1000)

  # save the last uri
  tools.addUri( me, application_category_uri )

  # subsitute into template
  category = tools.getTemplate( me )

  # save the result
  tools.addResult( me, category % locals() )

####################################################################################
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

  # save the last uri
  tools.addUri( me, application_uri )

  # subsitute into template
  app = tools.getTemplate( me )

  # save the result
  tools.addResult( me, app % locals() )


####################################################################################
def generateWebHistory(index):
  me = 'nfo#WebHistory'
  webhistory_uri       = 'urn:webhistory:%d' % index
  webhistory_title     = 'Web history %d' % (index % 1000)
  webhistory_date      = tools.getDateNowString()
  webhistory_created   = webhistory_date
  webhistory_modified  = webhistory_date
  webhistory_counter   = '%d' % (index % 10)
  webhistory_url       = 'http://www.%d.com/' % index

  # save the last uri
  tools.addUri( me, webhistory_uri )

  # subsitute into template
  data = tools.getTemplate( me )

  # save the result
  tools.addResult( me, data % locals() )

