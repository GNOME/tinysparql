# -*- coding: utf-8 -*-

import tools
import gen_data as gen

####################################################################################
nmo_MailAccount = '''
<%(account_uri)s> a nmo:MailAccount ;
    nmo:accountName "%(account_name)s" ;
    nmo:fromAddress %(account_from_address_uri)s ;
    nmo:signature   "%(account_signature)s" .
'''
def generateMailAccount(index):
  me = 'nmo#MailAccount'
  account_uri              = 'qmf://groove.nokia.com/accounts#%d' % index
  account_from_address_uri = '<from:%d>' % index
  account_name             = 'Account %d' % (index % 1000)
  account_signature        = 'Signature %d' % (index % 1000)

  tools.addItem( me, account_uri, nmo_MailAccount % locals() )

####################################################################################
nmo_MailFolder = '''
<%(mailfolder_uri)s> a nmo:MailFolder, nmo:MailboxDataObject ;
    tracker:available  true ;
    nie:relatedTo      %(mailfolder_account)s ;
    nmo:folderName    "%(mailfolder_name)s" .
'''
def generateMailFolder(index):
  me = 'nmo#MailFolder'
  mailfolder_uri     = 'qmf://groove.nokia.com/folder#%d' % index
  mailfolder_account = '<%s>' % tools.getRandomUri( 'nmo#MailAccount' )
  mailfolder_name    = 'Folder %d' % (index % 1000)

  tools.addItem( me, mailfolder_uri, nmo_MailFolder % locals() )

####################################################################################
nmo_Email = '''
<%(email_uri)s> a nmo:Email, nmo:MailboxDataObject ;
    nie:mimeType         "%(email_mime)s" ;
    nie:relatedTo        <%(email_account)s> ;
    nie:isStoredAs       <%(email_stored_as)s> ;
    nie:isLogicalPartOf  <%(email_folder)s> ;
    nie:contentSize      "%(email_size)s" ;
    nie:plainTextContent "%(email_content)s" ;
    nmo:recipient        <%(email_to)s> ;
    nmo:to               <%(email_to)s> ;
    nmo:messageId        "%(email_id)s" ;
    nmo:messageSubject   "%(email_subject)s" ;
    nmo:receivedDate     "%(email_received)s" ;
    nmo:sender           <%(email_from)s> ;
    nmo:from             <%(email_from)s> ;
    nmo:isDraft          "%(email_draft)s" ;
    nmo:isDeleted        "%(email_deleted)s" ;
    nmo:sentDate         "%(email_sent)s" ;
    tracker:available     true .
'''
def generateEmail(index):
  me = 'nmo#Email'
  email_uri       = 'qmf://groove.nokia.com/email%d' % index
  email_mime      = 'multipart/mixed'
  email_stored_as = 'urn:uuid:XX:%d' % index
  email_account   = tools.getRandomUri( 'nmo#MailAccount' )
  email_folder    = tools.getRandomUri( 'nmo#MailFolder' )
  email_size      = str(index)
  email_to        = tools.getRandomUri( 'nco#ContactEmail' )
  email_id        = str(index)
  email_subject   = 'Subject %d' % (index % 1000)
  email_received  = '%d-%02d-%02dT01:01:01Z' % (2000 + (index % 10), (index % 12) + 1, (index % 25) + 1)
  email_from      = tools.getRandomUri( 'nco#ContactEmail' )
  email_sent      = '%d-%02d-%02dT01:01:02Z' % (2000 + (index % 10), (index % 12) + 1, (index % 25) + 1)
  email_content   = gen.create_text(2,30)
  email_draft     = 'false'
  email_deleted   = 'false'

  tools.addItem( me, email_uri, nmo_Email % locals() )

####################################################################################
nmo_CommunicationChannel = '''
<%(channel_uri)s> a nmo:CommunicationChannel;
  nie:subject                "%(channel_subject)s";
  nie:informationElementDate "%(channel_date)s";
  nie:contentLastModified    "%(channel_modified)s";
  nmo:lastMessageDate        "%(channel_last_message)s";
  nmo:hasParticipant         <%(channel_participant)s> .
'''
def generateCommunicationChannel(index):
  me = 'nmo#CommunicationChannel'
  channel_uri          = 'urn:channel:%d' % index
  channel_subject      = '/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0'
  channel_date         = tools.now
  channel_modified     = channel_date
  channel_last_message = channel_date
  channel_participant  = tools.getRandomUri( 'nco#PersonContact' )

  tools.addItem( me, channel_uri, nmo_CommunicationChannel % locals() )

##################################################################

nmo_IMMessage = '''
<%(immessage_uri)s> a nmo:IMMessage;
  nie:plainTextContent       "%(immessage_content)s" ;
  nie:informationElementDate "%(immessage_date)s" ;
  nie:contentLastModified    "%(immessage_modified)s" ;
  nie:plainTextContent       "%(immessage_content)s" ;
  nmo:from                   <%(immessage_from)s> ;
  nmo:to                     <%(immessage_to)s> ;
  nmo:isDraft                "%(immessage_draft)s" ;
  nmo:isRead                 "%(immessage_read)s" ;
  nmo:isSent                 "%(immessage_sent)s" ;
  nmo:messageId              "%(immessage_message_id)s" ;
  nmo:receivedDate           "%(immessage_received)s" ;
  nmo:sentDate               "%(immessage_sent_date)s" ;
  nmo:isDeleted              "%(immessage_deleted)s" ;
  nmo:communicationChannel   <%(immessage_channel_uri)s> .
'''
def generateIMMessage(index):
  me = 'nmo#IMMessage'
  immessage_uri         = 'urn:immessage:%d' % index
  immessage_content     = 'Lorem IM Ipsum %d' % index
  immessage_date        = tools.now
  immessage_modified    = tools.now
  immessage_from        = tools.getRandomUri( 'nco#ContactIM' )
  immessage_to          = tools.getRandomUri( 'nco#ContactIM' )
  immessage_draft       = ('false', 'true')[index % 2]
  immessage_read        = ('false', 'true')[index % 2]
  immessage_sent        = 'true'
  immessage_deleted     = 'false'
  immessage_message_id  = '%d' % index
  immessage_received    = tools.now
  immessage_sent_date   = tools.now
  immessage_channel_uri = tools.getRandomUri( 'nmo#CommunicationChannel' )
  immessage_content     = gen.create_text( 1, 2 )

  tools.addItem( me, immessage_uri, nmo_IMMessage % locals() )

####################################################################################
nmo_SMSMessage = '''
<%(smsmessage_uri)s> a nmo:SMSMessage;
  nmo:smsId                  "%(smsmessage_id)s" ;
  nie:plainTextContent       "%(smsmessage_content)s";
  nie:informationElementDate "%(smsmessage_date)s";
  nie:contentLastModified    "%(smsmessage_modified)s";
  nie:plainTextContent       "%(smsmessage_content)s" ;
  nmo:from                   <%(smsmessage_from)s>  ;
  nmo:to                     <%(smsmessage_to)s>  ;
  nmo:isDraft                "%(smsmessage_draft)s" ;
  nmo:isRead                 "%(smsmessage_read)s" ;
  nmo:isSent                 "%(smsmessage_sent)s" ;
  nmo:messageId              "%(smsmessage_message_id)s" ;
  nmo:receivedDate           "%(smsmessage_received)s" ;
  nmo:sentDate               "%(smsmessage_sent_date)s" ;
  nmo:isDeleted              "%(smsmessage_deleted)s" ;
  nmo:communicationChannel   <%(smsmessage_channel_uri)s> .
'''
def generateSMSMessage(index):
  me = 'nmo#SMSMessage'
  smsmessage_uri         = 'urn:sms:%d' % index
  smsmessage_id          = '%d' % index
  smsmessage_content     = 'Lorem SMS Ipsum %d' % index
  smsmessage_date        = tools.now
  smsmessage_modified    = tools.now
  smsmessage_from        = tools.getRandomUri( 'nco#PersonContact' )
  smsmessage_to          = tools.getRandomUri( 'nco#PersonContact' )
  smsmessage_draft       = ('false', 'true')[index % 2]
  smsmessage_read        = ('false', 'true')[index % 2]
  smsmessage_sent        = 'true'
  smsmessage_deleted     = 'false'
  smsmessage_message_id  = '%d' % index
  smsmessage_received    = tools.now
  smsmessage_sent_date   = tools.now
  smsmessage_channel_uri = tools.getRandomUri( 'nmo#CommunicationChannel' )
  smsmessage_content     = gen.create_text( 1, 5 )

  tools.addItem( me, smsmessage_uri, nmo_SMSMessage % locals() )

####################################################################################
nmo_Call = '''
<%(call_uri)s> a nmo:Call;
  nie:subject                "%(call_subject)s";
  nie:informationElementDate "%(call_date)s";
  nie:contentLastModified    "%(call_modified)s";
  nmo:from                   <%(call_from)s>  ;
  nmo:to                     <%(call_to)s>  ;
  nmo:isRead                 "%(call_read)s" ;
  nmo:isSent                 "%(call_sent)s" ;
  nmo:receivedDate           "%(call_received)s" ;
  nmo:sentDate               "%(call_sent_date)s" ;
  nmo:duration               "%(call_duration)s" .
'''
def generateCall(index):
  me = 'nmo#Call'
  call_uri         = 'urn:call:%d' % index
  call_subject     = 'Subject %d' % index
  call_date        = tools.now
  call_modified    = tools.now
  call_from        = tools.getRandomUri( 'nco#ContactCall' )
  call_to          = tools.getRandomUri( 'nco#ContactCall' )
  call_read        = 'true'
  call_sent        = 'true'
  call_received    = tools.now
  call_sent_date   = tools.now
  call_duration    = '%d' % ( 10 + (index % 3600) )

  tools.addItem( me, call_uri, nmo_Call % locals() )
