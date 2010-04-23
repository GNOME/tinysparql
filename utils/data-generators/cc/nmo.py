# -*- coding: utf-8 -*-

import tools
import gen_data as gen

####################################################################################
def generateMailAccount(index):
  me = 'nmo#MailAccount'
  account_uri              = 'qmf://groove.nokia.com/accounts#%d' % index
  account_from_address_uri = '<from:%d>' % index
  account_name             = 'Account %d' % (index % 1000)
  account_signature        = 'Signature %d' % (index % 1000)

  # save the last uri
  tools.addUri( me, account_uri )

  # subsitute into template
  account = tools.getTemplate( me )

  # save the result
  tools.addResult( me, account % locals() )


####################################################################################
def generateMailFolder(index):
  me = 'nmo#MailFolder'
  mailfolder_uri     = 'qmf://groove.nokia.com/folder#%d' % index
  mailfolder_account = '<%s>' % tools.getRandomUri( 'nmo#MailAccount' )
  mailfolder_name    = 'Folder %d' % (index % 1000)

  # save the last uri
  tools.addUri( me, mailfolder_uri )

  # subsitute into template
  folder = tools.getTemplate( me )

  # save the result
  tools.addResult( me, folder % locals() )

####################################################################################
def generateEmail(index):
  me = 'nmo#Email'
  email_uri       = 'qmf://groove.nokia.com/email%d' % index
  email_mime      = 'multipart/mixed'
  email_stored_as = 'urn:uuid:XX:%d' % index
  email_account   = tools.getRandomUri( 'nmo#MailAccount' )
  email_folder    = tools.getRandomUri( 'nmo#MailFolder' )
  email_size      = str(index)
  email_recipient = tools.getRandomUri( 'nco#PersonContact' )
  email_id        = str(index)
  email_subject   = 'Subject %d' % (index % 1000)
  email_received  = '%d-%02d-%02dT01:01:01Z' % (2000 + (index % 10), (index % 12) + 1, (index % 25) + 1)
  email_sender    = tools.getRandomUri( 'nco#PersonContact' )
  email_sent      = '%d-%02d-%02dT01:01:02Z' % (2000 + (index % 10), (index % 12) + 1, (index % 25) + 1)
  email_content   = gen.create_text(2,30)

  # save the last uri
  tools.addUri( me, email_uri )

  # subsitute into template
  email = tools.getTemplate( me )

  # save the result
  tools.addResult( me, email % locals() )

####################################################################################
def generateCommunicationChannel(index):
  me = 'nmo#CommunicationChannel'
  channel_uri          = 'urn:channel:%d' % index
  channel_subject      = '/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0'
  channel_date         = tools.getDateNowString()
  channel_modified     = channel_date
  channel_last_message = channel_date
  channel_participiant = tools.getRandomUri( 'nco#PersonContact' )

  # save the last uri
  tools.addUri( me, channel_uri )

  # subsitute into template
  channel = tools.getTemplate( me )

  # save the result
  tools.addResult( me, channel % locals() )

####################################################################################
def generateIMMessage(index):
  me = 'nmo#IMMessage'
  immessage_uri         = 'urn:immessage:%d' % index
  immessage_content     = 'Lorem IM Ipsum %d' % index
  immessage_date        = tools.getDateNowString()
  immessage_modified    = tools.getDateNowString()
  immessage_from        = tools.getRandomUri( 'nco#PersonContact' )
  immessage_to          = tools.getRandomUri( 'nco#PersonContact' )
  immessage_draft       = ('false', 'true')[index % 2]
  immessage_read        = ('false', 'true')[index % 2]
  immessage_sent        = 'true'
  immessage_message_id  = '%d' % index
  immessage_received    = tools.getDateNowString()
  immessage_sent_date   = tools.getDateNowString()
  immessage_channel_uri = tools.getRandomUri( 'nmo#CommunicationChannel' )
  immessage_content     = gen.create_text( 1, 2 )

  # save the last uri
  tools.addUri( me, immessage_uri )

  # subsitute into template
  imm = tools.getTemplate( me )

  # save the result
  tools.addResult( me, imm % locals() )

####################################################################################
def generateSMSMessage(index):
  me = 'nmo#SMSMessage'
  smsmessage_uri         = 'urn:sms:%d' % index
  smsmessage_id          = '%d' % index
  smsmessage_content     = 'Lorem SMS Ipsum %d' % index
  smsmessage_date        = tools.getDateNowString()
  smsmessage_modified    = tools.getDateNowString()
  smsmessage_from        = tools.getRandomUri( 'nco#PersonContact' )
  smsmessage_to          = tools.getRandomUri( 'nco#PersonContact' )
  smsmessage_draft       = ('false', 'true')[index % 2]
  smsmessage_read        = ('false', 'true')[index % 2]
  smsmessage_sent        = 'true'
  smsmessage_message_id  = '%d' % index
  smsmessage_received    = tools.getDateNowString()
  smsmessage_sent_date   = tools.getDateNowString()
  smsmessage_channel_uri = tools.getRandomUri( 'nmo#CommunicationChannel' )
  smsmessage_content     = gen.create_text( 1, 5 )

  # save the last uri
  tools.addUri( me, smsmessage_uri )

  # subsitute into template
  sms = tools.getTemplate( me )

  # save the result
  tools.addResult( me, sms % locals() )

####################################################################################
def generateCall(index):
  me = 'nmo#Call'
  call_uri         = 'urn:call:%d' % index
  call_subject     = 'Subject %d' % index
  call_date        = tools.getDateNowString()
  call_modified    = tools.getDateNowString()
  call_from        = tools.getRandomUri( 'nco#PersonContact' )
  call_to          = tools.getRandomUri( 'nco#PersonContact' )
  call_read        = 'true'
  call_sent        = 'true'
  call_received    = tools.getDateNowString()
  call_sent_date   = tools.getDateNowString()
  call_duration    = '%d' % ( 10 + (index % 3600) )

  # save the last uri
  tools.addUri( me, call_uri )

  # subsitute into template
  call = tools.getTemplate( me )

  # save the result
  tools.addResult( me, call % locals() )
