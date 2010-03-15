# -*- coding: utf-8 -*-

import tools

####################################################################################
def generateEmailAddress(index):
  email_address = 'given%d.family%d@domain%d.com' % (index % 1000,index % 1000,index % 1000)
  email_address_uri = 'mailto:' + email_address

  # save the uri
  tools.addUri( 'nco#EmailAddress', email_address_uri )

  # substitute into template
  email = tools.getTemplate( 'nco#EmailAddress' )
  email = email.replace( '${email_address}', email_address )
  email = email.replace( '${email_address_uri}', email_address_uri )

  # save the result
  tools.addResult( 'nco#EmailAddress', email )

####################################################################################
def generatePhoneNumber(index):
  phonenumber = '+%d-555-%08d' %(index, index)
  phonenumber_uri = 'tel:' + phonenumber

  # save the last uri
  tools.addUri( 'nco#PhoneNumber', phonenumber_uri )

  # subsitute into template
  pn = tools.getTemplate( 'nco#PhoneNumber' )
  pn = pn.replace( '${phonenumber_uri}', phonenumber_uri )
  pn = pn.replace( '${phonenumber}', phonenumber )

  # save the result
  tools.addResult( 'nco#PhoneNumber', pn )


####################################################################################
def generatePostalAddress(index):
  postal_address_uri         = 'urn:pa:%d' % index
  postal_address_country     = 'Country %d' % (index % 1000)
  postal_address_region      = 'Region %d' % (index % 1000)
  postal_address_postal_code = '%05d' % (index % 100000)
  postal_address_city        = 'City %d' % (index % 1000)
  postal_address_street      = 'Demo Street %d' % (index % 100)

  # save the last uri
  tools.addUri( 'nco#PostalAddress', postal_address_uri )

  # subsitute into template
  pa = tools.getTemplate( 'nco#PostalAddress' )
  pa = pa.replace( '${postal_address_uri}', postal_address_uri )
  pa = pa.replace( '${postal_address_country}', postal_address_country )
  pa = pa.replace( '${postal_address_region}', postal_address_region )
  pa = pa.replace( '${postal_address_postal_code}', postal_address_postal_code )
  pa = pa.replace( '${postal_address_city}', postal_address_city )
  pa = pa.replace( '${postal_address_street}', postal_address_street )

  # save the result
  tools.addResult( 'nco#PostalAddress', pa )

####################################################################################
def generatePersonContact(index):
  contact_uri         = 'urn:uid:%d' % index
  contact_name_given  = 'Given%d' % (index % 1000)
  contact_name_family = 'Family%d' % (index % 1000)
  contact_birth_date  = '%d-%02d-%02dT%02d:%02d:%02dZ' % (1900 + (index % 100), (index % 12) + 1, (index % 25) + 1, (index % 12) + 1, (index % 12) + 1, (index % 12) + 1)
  email_address_uri   = '<%s>' % tools.getLastUri( 'nco#EmailAddress' )
  phonenumber_uri     = '<%s>' % tools.getLastUri( 'nco#PhoneNumber' )
  postal_address_uri  = '<%s>' % tools.getLastUri( 'nco#PostalAddress' )

  # save the uri
  tools.addUri( 'nco#PersonContact', contact_uri )

  # subsitute into template
  pc = tools.getTemplate( 'nco#PersonContact' )
  pc = pc.replace( '${contact_uri}', contact_uri )
  pc = pc.replace( '${contact_name_given}', contact_name_given )
  pc = pc.replace( '${contact_name_family}', contact_name_family )
  pc = pc.replace( '${contact_birth_date}', contact_birth_date )
  pc = pc.replace( '${email_address_uri}', email_address_uri )
  pc = pc.replace( '${phonenumber_uri}', phonenumber_uri )
  pc = pc.replace( '${postal_address_uri}', postal_address_uri )

  # save the result
  tools.addResult( 'nco#PersonContact', pc )
