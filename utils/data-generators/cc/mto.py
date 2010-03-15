# -*- coding: utf-8 -*-

import tools

####################################################################################
def generateTransferElement(index):
  transfer_uri         = 'http://www.sopranolive.org/contexts/tracker/generated_unique_id/%d' % (index % 1000)
  transfer_source      = tools.getRandomUri( 'nmm#Photo' ) 
  transfer_destination = 'http://www.uploadsite%d.com/item%d' % (index % 100, index )
  transfer_started     = tools.getDateNowString()
  transfer_completed   = tools.getDateNowString()
  transfer_state       = 'http://www.tracker-project.org/temp/mto#state-done'

  # save the last uri
  tools.addUri( 'mto#TransferElement', transfer_uri )

  # subsitute into template
  data = tools.getTemplate( 'mto#TransferElement' )
  data = data.replace( '${transfer_uri}', transfer_uri )
  data = data.replace( '${transfer_source}', transfer_source )
  data = data.replace( '${transfer_destination}', transfer_destination )
  data = data.replace( '${transfer_started}', transfer_started )
  data = data.replace( '${transfer_completed}', transfer_completed )
  data = data.replace( '${transfer_state}', transfer_state )

  # save the result
  tools.addResult( 'mto#TransferElement', data )

####################################################################################
def generateUploadTransfer(index):
  upload_uri     = 'mtransfer://%d' % index
  upload_state   = 'http://www.tracker-project.org/temp/mto#state-done'
  upload_method  = 'http://www.tracker-project.org/temp/mto#transfer-method-web'
  upload_created = tools.getDateNowString()
  upload_account = ('picasa', 'flickr', 'facebook', 'youtube')[ index % 4 ]

  # add some random transfers
  upload_transfers = ''
  for index in range (1, 2 + (index % 10)):
    upload_transfers += 'mto:transferList <%s> ;\n' % tools.getRandomUri( 'mto#TransferElement' )

  # save the last uri
  tools.addUri( 'mto#UploadTransfer', upload_uri )

  # subsitute into template
  data = tools.getTemplate( 'mto#UploadTransfer' )
  data = data.replace( '${upload_uri}', upload_uri )
  data = data.replace( '${upload_state}', upload_state )
  data = data.replace( '${upload_method}', upload_method )
  data = data.replace( '${upload_created}', upload_created )
  data = data.replace( '${upload_account}', upload_account )
  data = data.replace( '${upload_transfers}', upload_transfers )

  # save the result
  tools.addResult( 'mto#UploadTransfer', data )
