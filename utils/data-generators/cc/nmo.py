# -*- coding: utf-8 -*-

import tools
import gen_data as gen

####################################################################################
def generateMailAccount(index):
  account_uri              = 'qmf://groove.nokia.com/accounts#%d' % index
  account_from_address_uri = '<from:%d>' % index
  account_name             = 'Account %d' % (index % 1000)
  account_signature        = 'Signature %d' % (index % 1000)

  # save the last uri
  tools.addUri( 'nmo#MailAccount', account_uri )

  # subsitute into template
  account = tools.getTemplate( 'nmo#MailAccount' )
  account = account.replace( '${account_uri}', account_uri )
  account = account.replace( '${account_from_address_uri}', account_from_address_uri )
  account = account.replace( '${account_name}', account_name )
  account = account.replace( '${account_signature}', account_signature )

  # save the result
  tools.addResult( 'nmo#MailAccount', account )


####################################################################################
def generateMailFolder(index):
  mailfolder_uri     = 'qmf://groove.nokia.com/folder#%d' % index
  mailfolder_account = '<%s>' % tools.getRandomUri( 'nmo#MailAccount' )
  mailfolder_name    = 'Folder %d' % (index % 1000)

  # save the last uri
  tools.addUri( 'nmo#MailFolder', mailfolder_uri )

  # subsitute into template
  folder = tools.getTemplate( 'nmo#MailFolder' )
  folder = folder.replace( '${mailfolder_uri}', mailfolder_uri )
  folder = folder.replace( '${mailfolder_account}', mailfolder_account )
  folder = folder.replace( '${mailfolder_name}', mailfolder_name )

  # save the result
  tools.addResult( 'nmo#MailFolder', folder )

####################################################################################
def generateEmail(index):
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
  tools.addUri( 'nmo#Email', email_uri )

  # subsitute into template
  email = tools.getTemplate( 'nmo#Email' )
  email = email.replace( '${email_uri}', email_uri )
  email = email.replace( '${email_mime}', email_mime )
  email = email.replace( '${email_stored_as}', email_stored_as )
  email = email.replace( '${email_account}', email_account )
  email = email.replace( '${email_folder}', email_folder )
  email = email.replace( '${email_size}', email_size )
  email = email.replace( '${email_recipient}', email_recipient )
  email = email.replace( '${email_id}', email_id )
  email = email.replace( '${email_subject}', email_subject )
  email = email.replace( '${email_received}', email_received )
  email = email.replace( '${email_sender}', email_sender )
  email = email.replace( '${email_sent}', email_sent )
  email = email.replace( '${email_content}', email_content )

  # save the result
  tools.addResult( 'nmo#Email', email )

####################################################################################
def generateCommunicationChannel(index):
  channel_uri          = 'urn:channel:%d' % index
  channel_subject      = '/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0'
  channel_date         = tools.getDateNowString()
  channel_modified     = channel_date
  channel_last_message = channel_date
  channel_participiant = tools.getRandomUri( 'nco#PersonContact' )

  # save the last uri
  tools.addUri( 'nmo#CommunicationChannel', channel_uri )

  # subsitute into template
  channel = tools.getTemplate( 'nmo#CommunicationChannel' )
  channel = channel.replace( '${channel_uri}', channel_uri )
  channel = channel.replace( '${channel_subject}', channel_subject )
  channel = channel.replace( '${channel_date}', channel_date )
  channel = channel.replace( '${channel_modified}', channel_modified )
  channel = channel.replace( '${channel_last_message}', channel_last_message )
  channel = channel.replace( '${channel_participiant}', channel_participiant )

  # save the result
  tools.addResult( 'nmo#CommunicationChannel', channel )

####################################################################################
def generateIMMessage(index):
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
  tools.addUri( 'nmo#IMMessage', immessage_uri )

  # subsitute into template
  imm = tools.getTemplate( 'nmo#IMMessage' )
  imm = imm.replace( '${immessage_uri}', immessage_uri )
  imm = imm.replace( '${immessage_content}', immessage_content )
  imm = imm.replace( '${immessage_date}', immessage_date )
  imm = imm.replace( '${immessage_modified}', immessage_modified )
  imm = imm.replace( '${immessage_from}', immessage_from )
  imm = imm.replace( '${immessage_to}', immessage_to )
  imm = imm.replace( '${immessage_draft}', immessage_draft )
  imm = imm.replace( '${immessage_read}', immessage_read )
  imm = imm.replace( '${immessage_sent}', immessage_sent )
  imm = imm.replace( '${immessage_message_id}', immessage_message_id )
  imm = imm.replace( '${immessage_received}', immessage_received )
  imm = imm.replace( '${immessage_sent_date}', immessage_sent_date )
  imm = imm.replace( '${immessage_channel_uri}', immessage_channel_uri )
  imm = imm.replace( '${immessage_content}', immessage_content )

  # save the result
  tools.addResult( 'nmo#IMMessage', imm )

####################################################################################
def generateSMSMessage(index):
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
  tools.addUri( 'nmo#SMSMessage', smsmessage_uri )

  # subsitute into template
  sms = tools.getTemplate( 'nmo#SMSMessage' )
  sms = sms.replace( '${smsmessage_uri}', smsmessage_uri )
  sms = sms.replace( '${smsmessage_id}', smsmessage_id )
  sms = sms.replace( '${smsmessage_content}', smsmessage_content )
  sms = sms.replace( '${smsmessage_date}', smsmessage_date )
  sms = sms.replace( '${smsmessage_modified}', smsmessage_modified )
  sms = sms.replace( '${smsmessage_from}', smsmessage_from )
  sms = sms.replace( '${smsmessage_to}', smsmessage_to )
  sms = sms.replace( '${smsmessage_draft}', smsmessage_draft )
  sms = sms.replace( '${smsmessage_read}', smsmessage_read )
  sms = sms.replace( '${smsmessage_sent}', smsmessage_sent )
  sms = sms.replace( '${smsmessage_message_id}', smsmessage_message_id )
  sms = sms.replace( '${smsmessage_received}', smsmessage_received )
  sms = sms.replace( '${smsmessage_sent_date}', smsmessage_sent_date )
  sms = sms.replace( '${smsmessage_channel_uri}', smsmessage_channel_uri )
  sms = sms.replace( '${smsmessage_content}', smsmessage_content )

  # save the result
  tools.addResult( 'nmo#SMSMessage', sms )

####################################################################################
def generateCall(index):
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
  tools.addUri( 'nmo#Call', call_uri )

  # subsitute into template
  call = tools.getTemplate( 'nmo#Call' )
  call = call.replace( '${call_uri}', call_uri )
  call = call.replace( '${call_subject}', call_subject )
  call = call.replace( '${call_date}', call_date )
  call = call.replace( '${call_modified}', call_modified )
  call = call.replace( '${call_from}', call_from )
  call = call.replace( '${call_to}', call_to )
  call = call.replace( '${call_read}', call_read )
  call = call.replace( '${call_sent}', call_sent )
  call = call.replace( '${call_received}', call_received )
  call = call.replace( '${call_sent_date}', call_sent_date )
  call = call.replace( '${call_duration}', call_duration )

  # save the result
  tools.addResult( 'nmo#Call', call )
