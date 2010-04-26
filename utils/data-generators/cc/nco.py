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
nco_PostalAddress = '''
<%(postal_address_uri)s> a nco:PostalAddress;
    nco:country "%(postal_address_country)s" ;
    nco:region "%(postal_address_region)s" ;
    nco:postalcode "%(postal_address_postal_code)s" ;
    nco:locality "%(postal_address_city)s" ;
    nco:streetAddress "%(postal_address_street)s" .
'''
def generatePostalAddress(index):
  me = 'nco#PostalAddress'
  postal_address_uri         = 'urn:pa:%d' % index
  postal_address_country     = 'Country %d' % (index % 1000)
  postal_address_region      = 'Region %d' % (index % 1000)
  postal_address_postal_code = '%05d' % (index % 100000)
  postal_address_city        = 'City %d' % (index % 1000)
  postal_address_street      = 'Demo Street %d' % (index % 100)

  tools.addItem( me, postal_address_uri, nco_PostalAddress % locals() )

####################################################################################
nco_PersonContact = '''
<%(contact_uri)s> a nco:PersonContact;
    nco:fullname         "%(contact_name_given)s %(contact_name_family)s";
    nco:nameGiven        "%(contact_name_given)s";
    nco:nameFamily       "%(contact_name_family)s";
    nco:birthDate        "%(contact_birth_date)s";
    nco:hasEmailAddress   %(email_address_uri)s;
    nco:hasPhoneNumber    %(phonenumber_uri)s;
    nco:hasPostalAddress  %(postal_address_uri)s .
'''
def generatePersonContact(index):
  me = 'nco#PersonContact'
  contact_uri         = 'urn:uid:%d' % index
  contact_name_given  = 'Given%d' % (index % 1000)
  contact_name_family = 'Family%d' % (index % 1000)
  contact_birth_date  = '%d-%02d-%02dT%02d:%02d:%02dZ' % (1900 + (index % 100), (index % 12) + 1, (index % 25) + 1, (index % 12) + 1, (index % 12) + 1, (index % 12) + 1)
  email_address_uri   = '<%s>' % tools.getLastUri( 'nco#EmailAddress' )
  phonenumber_uri     = '<%s>' % tools.getLastUri( 'nco#PhoneNumber' )
  postal_address_uri  = '<%s>' % tools.getLastUri( 'nco#PostalAddress' )

  tools.addItem( me, contact_uri, nco_PersonContact % locals() )
