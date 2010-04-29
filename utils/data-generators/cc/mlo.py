# -*- coding: utf-8 -*-

import tools
import gen_data as gen

####################################################################################
mlo_GeoPoint = '''
<%(geopoint_uri)s> a mlo:GeoPoint ;
    mlo:latitude    "%(geopoint_latitude)s" ;
    mlo:longitude   "%(geopoint_longitude)s" ;
    mlo:altitude    "%(geopoint_altitude)s" .
'''
def generateGeoPoint(index):
  me = 'mlo#GeoPoint'
  geopoint_uri             = 'urn:geopoint:%d' % index
  geopoint_latitude        = '%f' % (((index % 360) - 180)/2.0)
  geopoint_longitude       = '%f' % ((index % 720)/4.0)
  geopoint_altitude        = '%f' % (index % 1000)

  tools.addItem( me, geopoint_uri, mlo_GeoPoint % locals() )

####################################################################################
mlo_LocationBoundingBox = '''
<%(boundingbox_geopoint_se_uri)s> a mlo:GeoPoint ;
    mlo:latitude    "%(boundingbox_geopoint_se_latitude)s" ;
    mlo:longitude   "%(boundingbox_geopoint_se_longitude)s" ;
    mlo:altitude    "%(boundingbox_geopoint_se_altitude)s" .

<%(boundingbox_geopoint_nw_uri)s> a mlo:GeoPoint ;
    mlo:latitude    "%(boundingbox_geopoint_nw_latitude)s" ;
    mlo:longitude   "%(boundingbox_geopoint_nw_longitude)s" ;
    mlo:altitude    "%(boundingbox_geopoint_nw_altitude)s" .

<%(boundingbox_uri)s> a mlo:LocationBoundingBox ;
    mlo:bbSouthEast    <%(boundingbox_geopoint_se_uri)s> ;
    mlo:bbNorthWest    <%(boundingbox_geopoint_nw_uri)s> .
'''
def generateLocationBoundingBox(index):
  me = 'mlo#LocationBoundingBox'
  boundingbox_uri             = 'urn:boundingbox:%d' % index

  boundingbox_geopoint_se_uri             = 'urn:boundingbox:se:%d' % index
  boundingbox_geopoint_se_latitude        = '%f' % (((index % 700) - 350)/4.0)
  boundingbox_geopoint_se_longitude       = '%f' % (((index % 350) + 10) /2.0)
  boundingbox_geopoint_se_altitude        = '%f' % (index % 1000)

  boundingbox_geopoint_nw_uri             = 'urn:boundingbox:nw:%d' % index
  boundingbox_geopoint_nw_latitude        = '%f' % (((index % 700) - 340)/4.0)
  boundingbox_geopoint_nw_longitude       = '%f' % (((index % 350))/2.0)
  boundingbox_geopoint_nw_altitude        = '%f' % (index % 1000)

  tools.addItem( me, boundingbox_uri, mlo_LocationBoundingBox % locals() )

####################################################################################
mlo_GeoLocation = '''
<%(geolocation_uri)s> a mlo:GeoLocation;
  %(geolocation_saveas)s ;
  nie:comment         "%(geolocation_comment)s" .
'''
mlo_GeoLocation_saveas_geopoint      = '''mlo:asGeoPoint      <%(geolocation_geopoint)s>'''
mlo_GeoLocation_saveas_boundingbox   = '''mlo:asBoundingBox   <%(geolocation_boundingbox)s>'''
mlo_GeoLocation_saveas_postaladdress = '''mlo:asPostalAddress <%(geolocation_postaladdress)s>'''

def generateGeoLocation(index):
  me = 'mlo#GeoLocation'
  geolocation_uri                     = 'urn:geolocation:%d' % index

  geolocation_geopoint                = tools.getLastUri( 'mlo#GeoPoint' )
  geolocation_boundingbox             = tools.getLastUri( 'mlo#LocationBoundingBox' )
  geolocation_postaladdress           = tools.getRandomUri( 'nco#PostalAddress' )

  geolocation_saveas                  = (mlo_GeoLocation_saveas_boundingbox % locals(), mlo_GeoLocation_saveas_geopoint % locals(), mlo_GeoLocation_saveas_postaladdress % locals()) [ index %3 ]

  geolocation_comment                 = 'geolocation %d' % index

  tools.addItem( me, geolocation_uri, mlo_GeoLocation % locals() )


####################################################################################
mlo_Landmark = '''
<%(landmark_uri)s> a mlo:Landmark ;
  nie:title              "%(landmark_title)s" ;
  nie:description        "%(landmark_description)s" ;
  mlo:location           <%(landmark_location)s> .

'''
def generateLandmark(index):
  me = 'mlo#Landmark'
  landmark_uri             = 'urn:landmark:%d' % index
  landmark_title           = 'Landmark %d' % index
  landmark_description     = 'Landmark %d description' % index
  landmark_location        = tools.getLastUri( 'mlo#GeoLocation' )

  tools.addItem( me, landmark_uri, mlo_Landmark % locals() )

