# -*- coding: utf-8 -*-

import tools

####################################################################################
nco_EmailAddress = '''
<%(email_address_uri)s> a nco:EmailAddress;
    nco:emailAddress "%(email_address)s".
'''
def generateEmailAddress(index):
  me = 'nco#EmailAddress'
  email_address = 'given%d.family%d@domain%d.com' % (index % 1000,index % 1000,index % 1000)
  email_address_uri = 'mailto:' + email_address

  tools.addItem( me, email_address_uri, nco_EmailAddress % locals() )

####################################################################################
nco_Contact_Email = '''
<%(emailcontact_uri)s> a nco:Contact;
    nco:fullname            "%(emailcontact_name_given)s %(emailcontact_name_family)s";
    nco:nickname            "%(emailcontact_nickname)s" ;
    nco:hasEmailAddress      %(emailcontact_email_address_uri)s .
'''
def generateContactEmail(index):
  me = 'nco#ContactEmail'
  emailcontact_uri              = 'urn:contact:email%d' % index
  emailcontact_name_given       = 'Given%d' % (index % 1000)
  emailcontact_name_family      = 'Family%d' % (index % 1000)
  emailcontact_nickname         = 'Nickname%d' % (index % 1000)
  emailcontact_email_address_uri= '<%s>' % tools.getLastUri( 'nco#EmailAddress' )

  tools.addItem( me, emailcontact_uri, nco_Contact_Email % locals() )

####################################################################################
nco_PhoneNumber = '''
<%(phonenumber_uri)s> a nco:CellPhoneNumber;
    nco:phoneNumber "%(phonenumber)s".
'''
def generatePhoneNumber(index):
  me = 'nco#PhoneNumber'
  phonenumber = '+%d-555-%08d' %(index, index)
  phonenumber_uri = 'tel:' + phonenumber

  tools.addItem( me, phonenumber_uri, nco_PhoneNumber % locals() )

####################################################################################

nco_Contact_Call = '''
<%(callcontact_uri)s> a nco:Contact;
    nco:fullname            "%(callcontact_name_given)s %(callcontact_name_family)s";
    nco:nickname            "%(callcontact_nickname)s" ;
    nco:hasPhoneNumber       %(callcontact_phonenumber_uri)s .
'''
def generateContactCall(index):
  me = 'nco#ContactCall'
  callcontact_uri              = 'urn:contact:call%d' % index
  callcontact_name_given       = 'Given%d' % (index % 1000)
  callcontact_name_family      = 'Family%d' % (index % 1000)
  callcontact_nickname         = 'Nickname%d' % (index % 1000)
  callcontact_phonenumber_uri  = '<%s>' % tools.getLastUri( 'nco#PhoneNumber' )

  tools.addItem( me, callcontact_uri, nco_Contact_Call % locals() )

####################################################################################

nco_PostalAddress = '''
<%(postal_address_uri)s> a nco:PostalAddress;
    nco:country         "%(postal_address_country)s" ;
    nco:pobox           "%(postal_address_pobox)s" ;
    nco:region          "%(postal_address_region)s" ;
    nco:postalcode      "%(postal_address_postal_code)s" ;
    nco:locality        "%(postal_address_city)s" ;
    nco:streetAddress   "%(postal_address_street)s" .
'''
def generatePostalAddress(index):
  me = 'nco#PostalAddress'
  postal_address_uri         = 'urn:pa:%d' % index
  postal_address_country     = 'Country %d' % (index % 1000)
  postal_address_locality    = 'Locality %d' % (index % 1000)
  postal_address_pobox       = str(index)
  postal_address_region      = 'Region %d' % (index % 1000)
  postal_address_postal_code = '%05d' % (index % 100000)
  postal_address_city        = 'City %d' % (index % 1000)
  postal_address_street      = 'Demo Street %d' % (index % 100)

  tools.addItem( me, postal_address_uri, nco_PostalAddress % locals() )

####################################################################################
nco_IMAddress = '''
<%(im_address_uri)s> a nco:IMAddress;
    nco:imID         "%(im_address_imid)s" ;
    nco:imCapability  %(im_address_capability)s .
'''
def generateIMAddress(index):
  me = 'nco#IMAddress'
  im_address_uri          = 'urn:ima:%d' % index
  im_address_imid         = 'IM ID %d' % (index % 1000)
  im_address_capability   = '<%s>' % ('nco:im-capability-text-chat', 'nco:im-capability-audio-calls') [ index %2 ]
  tools.addItem( me, im_address_uri, nco_IMAddress % locals() )

####################################################################################

nco_Contact_IM = '''
<%(imcontact_uri)s> a nco:Contact;
    nco:fullname            "%(imcontact_name_given)s %(imcontact_name_family)s";
    nco:nickname            "%(imcontact_nickname)s" ;
    nco:hasIMAddress         %(imcontact_imaddress_uri)s .
'''
def generateContactIM(index):
  me = 'nco#ContactIM'
  imcontact_uri              = 'urn:contact:im%d' % index
  imcontact_name_given       = 'Given%d' % (index % 1000)
  imcontact_name_family      = 'Family%d' % (index % 1000)
  imcontact_nickname         = 'Nickname%d' % (index % 1000)
  imcontact_imaddress_uri    = '<%s>' % tools.getLastUri( 'nco#IMAddress' )

  tools.addItem( me, imcontact_uri, nco_Contact_IM % locals() )

####################################################################################
nco_PersonContact = '''
<%(contact_uri)s> a nco:PersonContact;
    nco:fullname            "%(contact_name_given)s %(contact_name_family)s";
    nco:nameGiven           "%(contact_name_given)s";
    nco:nameFamily          "%(contact_name_family)s";
    nco:nameAdditional      "%(contact_name_additional)s" ;
    nco:nickname            "%(contact_nickname)s" ;
    nco:nameHonorificPrefix "%(contact_honorific_prefix)s" ;
    nco:nameHonorificSuffix "%(contact_honorific_suffix)s" ;
    nco:birthDate           "%(contact_birth_date)s" ;
    nco:gender               %(contact_gender)s ;
    nco:contactUID          "%(contact_uid)s" ;
    nco:note                "%(contact_note)s" ;
    nco:hasEmailAddress      %(email_address_uri)s ;
    nco:hasPhoneNumber       %(phonenumber_uri)s ;
    nco:hasPostalAddress     %(postal_address_uri)s ;
    nco:hasIMAddress         %(im_address_uri)s ;
    nie:contentCreated      "%(contact_created)s" ;
    nie:contentLastModified "%(contact_modified)s"  .
'''
def generatePersonContact(index):
  me = 'nco#PersonContact'
  contact_uri              = 'urn:uid:%d' % index
  contact_name_given       = 'Given%d' % (index % 1000)
  contact_name_family      = 'Family%d' % (index % 1000)
  contact_name_additional  = 'Additional%d' % (index % 1000)
  contact_nickname         = 'Nickname%d' % (index % 1000)
  contact_honorific_prefix = ('Sir', 'Master') [index % 2]
  contact_honorific_suffix = ('PhD', 'Deceased') [index % 2]
  contact_birth_date       = '%d-%02d-%02dT%02d:%02d:%02dZ' % (1900 + (index % 100), (index % 12) + 1, (index % 25) + 1, (index % 12) + 1, (index % 12) + 1, (index % 12) + 1)
  contact_gender           = '<%s>' % ('nco:gender-female', 'nco:gender-female') [index % 2]
  contact_uid              = 'uid:contact:%d' % index
  contact_note             = 'Note number %d' % index
  email_address_uri        = '<%s>' % tools.getLastUri( 'nco#EmailAddress' )
  phonenumber_uri          = '<%s>' % tools.getLastUri( 'nco#PhoneNumber' )
  postal_address_uri       = '<%s>' % tools.getLastUri( 'nco#PostalAddress' )
  im_address_uri           = '<%s>' % tools.getLastUri( 'nco#IMAddress' )
  contact_created          = '%d-%02d-%02dT%02d:%02d:%02dZ' % (1950 + (index % 50), (index % 12) + 1, (index % 25) + 1, (index % 12) + 1, (index % 12) + 1, (index % 12) + 1)
  contact_modified         = '%d-%02d-%02dT%02d:%02d:%02dZ' % (1960 + (index % 40), (index % 12) + 1, (index % 25) + 1, (index % 12) + 1, (index % 12) + 1, (index % 12) + 1)

  tools.addItem( me, contact_uri, nco_PersonContact % locals() )
