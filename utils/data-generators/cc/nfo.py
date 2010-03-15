# -*- coding: utf-8 -*-

import tools
import gen_data as gen

####################################################################################
def generatePlainTextDocument(index):
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
  tools.addUri( 'nfo#PlainTextDocument', plaintext_document_uri )

  # subsitute into template
  doc = tools.getTemplate( 'nfo#PlainTextDocument' )
  doc = doc.replace( '${plaintext_document_uri}', plaintext_document_uri )
  doc = doc.replace( '${plaintext_document_url}', plaintext_document_url )
  doc = doc.replace( '${plaintext_document_datasource}', plaintext_document_datasource )
  doc = doc.replace( '${plaintext_document_filename}', plaintext_document_filename )
  doc = doc.replace( '${plaintext_document_container}', plaintext_document_container )
  doc = doc.replace( '${plaintext_document_mime}', plaintext_document_mime )
  doc = doc.replace( '${plaintext_document_content}', plaintext_document_content )
  doc = doc.replace( '${plaintext_document_stored_as}', plaintext_document_stored_as )
  doc = doc.replace( '${plaintext_document_last_accessed}', plaintext_document_last_accessed )
  doc = doc.replace( '${plaintext_document_last_modified}', plaintext_document_last_modified )
  doc = doc.replace( '${plaintext_document_size}', plaintext_document_size )

  # save the result
  tools.addResult( 'nfo#PlainTextDocument', doc )

####################################################################################
def generateSoftwareCategory(index):
  application_category_uri   = 'urn:software-category:%d' % index
  application_category_title = 'Category %d' % (index % 1000)

  # save the last uri
  tools.addUri( 'nfo#SoftwareCategory', application_category_uri )

  # subsitute into template
  category = tools.getTemplate( 'nfo#SoftwareCategory' )
  category = category.replace( '${application_category_uri}', application_category_uri )
  category = category.replace( '${application_category_title}', application_category_title )

  # save the result
  tools.addResult( 'nfo#SoftwareCategory', category )

####################################################################################
def generateSoftwareApplication(index):
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
  tools.addUri( 'nfo#SoftwareApplication', application_uri )

  # subsitute into template
  app = tools.getTemplate( 'nfo#SoftwareApplication' )
  app = app.replace( '${application_cmdline}', application_cmdline )
  app = app.replace( '${application_filename}', application_filename )
  app = app.replace( '${application_uri}', application_uri )
  app = app.replace( '${application_title}', application_title )
  app = app.replace( '${application_part_of}', application_part_of )
  app = app.replace( '${application_url}', application_url )
  app = app.replace( '${application_datasource}', application_datasource )
  app = app.replace( '${application_last_modified}', application_last_modified )
  app = app.replace( '${application_icon}', application_icon )

  # save the result
  tools.addResult( 'nfo#SoftwareApplication', app )


####################################################################################
def generateWebHistory(index):
  webhistory_uri       = 'urn:webhistory:%d' % index
  webhistory_title     = 'Web history %d' % (index % 1000)
  webhistory_date      = tools.getDateNowString()
  webhistory_created   = webhistory_date
  webhistory_modified  = webhistory_date
  webhistory_counter   = '%d' % (index % 10)
  webhistory_url       = 'http://www.%d.com/' % index

  # save the last uri
  tools.addUri( 'nfo#WebHistory', webhistory_uri )

  # subsitute into template
  data = tools.getTemplate( 'nfo#WebHistory' )
  data = data.replace( '${webhistory_uri}', webhistory_uri )
  data = data.replace( '${webhistory_title}', webhistory_title )
  data = data.replace( '${webhistory_date}', webhistory_date )
  data = data.replace( '${webhistory_created}', webhistory_created )
  data = data.replace( '${webhistory_modified}', webhistory_modified )
  data = data.replace( '${webhistory_counter}', webhistory_counter )
  data = data.replace( '${webhistory_url}', webhistory_url )

  # save the result
  tools.addResult( 'nfo#WebHistory', data )

