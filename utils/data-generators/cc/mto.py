# -*- coding: utf-8 -*-

import tools

####################################################################################
def generateTransferElement(index):
  me = 'mto#TransferElement'
  transfer_uri         = 'http://www.sopranolive.org/contexts/tracker/generated_unique_id/%d' % (index % 1000)
  transfer_source      = tools.getRandomUri( 'nmm#Photo' )
  transfer_destination = 'http://www.uploadsite%d.com/item%d' % (index % 100, index )
  transfer_started     = tools.getDateNowString()
  transfer_completed   = tools.getDateNowString()
  transfer_state       = 'http://www.tracker-project.org/temp/mto#state-done'

  # save the last uri
  tools.addUri( me, transfer_uri )

  # subsitute into template
  data = tools.getTemplate( me )

  # save the result
  tools.addResult( me, data % locals() )

####################################################################################
def generateUploadTransfer(index):
  me = 'mto#UploadTransfer'
  upload_uri     = 'mtransfer://%d' % index
  upload_state   = 'http://www.tracker-project.org/temp/mto#state-done'
  upload_method  = 'http://www.tracker-project.org/temp/mto#transfer-method-web'
  upload_created = tools.getDateNowString()
  upload_account = ('picasa', 'flickr', 'facebook', 'youtube')[ index % 4 ]

  # add some random transfers
  upload_transfers = ''
  for index in xrange (1, 2 + (index % 10)):
    upload_transfers += 'mto:transferList <%s> ;\n' % tools.getRandomUri( 'mto#TransferElement' )

  # save the last uri
  tools.addUri( me, upload_uri )

  # subsitute into template
  data = tools.getTemplate( me )
  # save the result
  tools.addResult( me, data % locals() )
