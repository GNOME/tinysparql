# -*- coding: utf-8 -*-

import tools
import gen_data as gen

####################################################################################
slo_GeoLocation = '''
<%(geolocation_uri)s> a slo:GeoLocation;
  %(geolocation_saveas)s
  slo:altitude                  "%(geolocation_altitude)s" ;
  nie:comment                   "%(geolocation_comment)s" .
'''

slo_GeoLocation_saveas_point         = '''
  slo:longitude                 "%(geolocation_longitude)s" ;
  slo:latitude                  "%(geolocation_latitude)s" ;
  slo:horizontalAccuracy        "%(geolocation_hacc)s" ;
  slo:verticalAccuracy          "%(geolocation_vacc)s" ;
  slo:radius                    "%(geolocation_radius)s" ;
'''

slo_GeoLocation_saveas_boundingbox   = '''
  slo:boundingLongitudeMax      "%(geolocation_longitudemax)s" ;
  slo:boundingLongitudeMin      "%(geolocation_longitudemin)s" ;
  slo:boundingLatitudeMax       "%(geolocation_latitudemax)s" ;
  slo:boundingLatitudeMin       "%(geolocation_latitudemin)s" ;
'''

slo_GeoLocation_saveas_postaladdress = '''
  slo:postalAddress             <%(geolocation_postaladdress)s> ;

'''

def generateGeoLocation(index):
  me = 'slo#GeoLocation'
  geolocation_uri                     = 'urn:slogeoloc:%d' % index
  geolocation_comment                 = 'Comment geolocation %d' % index
  geolocation_altitude                = '%f' % (index % 1000)

  geolocation_latitude                = '%f' % (((index % 360) - 180)/2.0)
  geolocation_longitude               = '%f' % ((index % 720)/4.0)
  geolocation_hacc                    = '3'
  geolocation_vacc                    = '3'
  geolocation_radius                  = '10'

  geolocation_latitudemax             = '%f' % (((index % 700) - 340)/4.0)
  geolocation_latitudemin             = '%f' % (((index % 700) - 350)/4.0)
  geolocation_longitudemax            = '%f' % (((index % 350) + 10) /2.0)
  geolocation_longitudemin            = '%f' % (((index % 350))/2.0)

  geolocation_postaladdress           = tools.getRandomUri( 'nco#PostalAddress' )

  geolocation_saveas                  = (slo_GeoLocation_saveas_boundingbox % locals(),slo_GeoLocation_saveas_point % locals(),slo_GeoLocation_saveas_postaladdress % locals()) [ index %3 ]

  tools.addItem( me, geolocation_uri, slo_GeoLocation % locals() )

####################################################################################
slo_Landmark = '''
<%(landmark_uri)s> a slo:Landmark ;
  nie:title              "%(landmark_title)s" ;
  nie:description        "%(landmark_description)s" ;
  slo:location           <%(landmark_location)s> .

'''
def generateLandmark(index):
  me = 'slo#Landmark'
  landmark_uri             = 'urn:slolandmark:%d' % index
  landmark_title           = 'Landmark %d' % index
  landmark_description     = 'Landmark %d description' % index
  landmark_location        = tools.getLastUri( 'slo#GeoLocation' )

  tools.addItem( me, landmark_uri, slo_Landmark % locals() )

