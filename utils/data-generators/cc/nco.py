# -*- coding: utf-8 -*-

import tools

####################################################################################
def generateEmailAddress(index):
  me = 'nco#EmailAddress'
  email_address = 'given%d.family%d@domain%d.com' % (index % 1000,index % 1000,index % 1000)
  email_address_uri = 'mailto:' + email_address

  # save the uri
  tools.addUri( me, email_address_uri )

  # substitute into template
  email = tools.getTemplate( me )

  # save the result
  tools.addResult( me, email % locals() )

####################################################################################
def generatePhoneNumber(index):
  me = 'nco#PhoneNumber'
  phonenumber = '+%d-555-%08d' %(index, index)
  phonenumber_uri = 'tel:' + phonenumber

  # save the last uri
  tools.addUri( me, phonenumber_uri )

  # subsitute into template
  pn = tools.getTemplate( me )

  # save the result
  tools.addResult( me, pn % locals() )


####################################################################################
def generatePostalAddress(index):
  me = 'nco#PostalAddress'
  postal_address_uri         = 'urn:pa:%d' % index
  postal_address_country     = 'Country %d' % (index % 1000)
  postal_address_region      = 'Region %d' % (index % 1000)
  postal_address_postal_code = '%05d' % (index % 100000)
  postal_address_city        = 'City %d' % (index % 1000)
  postal_address_street      = 'Demo Street %d' % (index % 100)

  # save the last uri
  tools.addUri( me, postal_address_uri )

  # subsitute into template
  pa = tools.getTemplate( me )

  # save the result
  tools.addResult( me, pa % locals() )

####################################################################################
def generatePersonContact(index):
  me = 'nco#PersonContact'
  contact_uri         = 'urn:uid:%d' % index
  contact_name_given  = 'Given%d' % (index % 1000)
  contact_name_family = 'Family%d' % (index % 1000)
  contact_birth_date  = '%d-%02d-%02dT%02d:%02d:%02dZ' % (1900 + (index % 100), (index % 12) + 1, (index % 25) + 1, (index % 12) + 1, (index % 12) + 1, (index % 12) + 1)
  email_address_uri   = '<%s>' % tools.getLastUri( 'nco#EmailAddress' )
  phonenumber_uri     = '<%s>' % tools.getLastUri( 'nco#PhoneNumber' )
  postal_address_uri  = '<%s>' % tools.getLastUri( 'nco#PostalAddress' )

  # save the uri
  tools.addUri( me, contact_uri )

  # subsitute into template
  pc = tools.getTemplate( me )

  # save the result
  tools.addResult( me, pc % locals() )
