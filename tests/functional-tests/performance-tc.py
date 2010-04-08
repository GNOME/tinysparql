#!/usr/bin/env python2.5
#
# Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

import sys,os,dbus,commands, signal
import unittest
import time
import random
import datetime

TRACKER = 'org.freedesktop.Tracker1'
TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources'
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources"



"""import .ttl files """
"""
def stats() :
    a1=commands.getoutput("tracker-stats |grep  %s " %(stats[i]))
    b1=a1.split()
    after=b1[2]
    return after

def import_ttl (music_ttl):
       1. Checking the tracker stats before importing the ttl file .
       2. Importing the ttl file .
       3. Check the tracker-stats after importing the ttl file.
       4. Check if the stats got changed.
    bus= dbus.SessionBus()
    imp_obj = bus.get_object('org.freedesktop.Tracker1','/org/freedesktop/Tracker1/Resources')
    imp_iface = dbus.Interface(imp_obj, 'org.freedesktop.Tracker1.Resources')
    #stats_obj = bus.get_object('org.freedesktop.Tracker1','/org/freedesktop/Tracker1/Statistics')
    #stats_iface = dbus.Interface(stats_obj, 'org.freedesktop.Tracker1.Statistics')

    ttl=['040-nmm_Artist.ttl']
    stats=['nmm:Artist','nmm:MusicAlbum']
    for i in range(len(ttl)) :
         file_ttl='file://' +music_ttl+'/'+ttl[i]
         a=commands.getoutput("tracker-stats | grep  %s " %(stats[i]) )
	 b=a.split()
         imp_iface.Load(file_ttl)
 	 a1=commands.getoutput("tracker-stats |grep  %s " %(stats[i]))
         b1=a1.split()
         after=b1[2]
	 while (t < 2):
	   t=stats()
           time.sleep(2)

"""






class TestUpdate (unittest.TestCase):

        def setUp(self):
                bus = dbus.SessionBus()
                tracker = bus.get_object(TRACKER, TRACKER_OBJ)
                self.resources = dbus.Interface (tracker,
                                                 dbus_interface=RESOURCES_IFACE)


""" email performance test cases """
class email(TestUpdate):


        def p_test_email_01(self):

		query = "SELECT ?m ?From  ?date ?email1 WHERE { \
                 	?m a  nmo:Email ; \
                 	nmo:receivedDate ?date ;\
                 	nmo:from ?From . ?from nco:hasEmailAddress ?email1 } LIMIT 10000"

		"""Query for emails """
        	start=time.time()

		result=self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying emails = %s " %elapse
		print "no. of items retrieved: %d" %len(result)




""" calls performance  test cases """
class calls(TestUpdate):


        def p_test_calls_01(self):

		query = "SELECT ?duration ?phonenumber WHERE {\
                   	?call  a  nmo:Call ;\
                   	nmo:duration ?duration ;\
                   	nmo:from [a nco:Contact ; nco:hasPhoneNumber ?phonenumber] }LIMIT 10000"

		"""Querying the duration of calls of contacts """
        	start=time.time()

		result=self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying duration of calls from phonenumbers  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)

        def p_test_calls_02(self):

		query = "SELECT ?name ?date ?number ?duration \
			WHERE {?m a nmo:Call; \
			nmo:sentDate ?date ; \
			nmo:duration ?duration; \
			nmo:to ?contact . \
			?contact a nco:PersonContact; \
			nco:hasPhoneNumber ?number . \
			OPTIONAL { \
			?contact a nco:PersonContact ; \
			nco:nameFamily ?name} \
			FILTER (?duration > 0) .} \
			ORDER BY desc(?date) LIMIT 1000"

		"""Querying the dialed calls"""
        	start=time.time()

		result=self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying dialed calls  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)


        def p_test_calls_03(self):

		query = "SELECT ?name ?date ?number ?duration \
			WHERE {?m a nmo:Call; \
			nmo:receivedDate ?date ; \
			nmo:duration ?duration; \
			nmo:from ?contact . \
			?contact a nco:PersonContact; \
			nco:hasPhoneNumber ?number . \
			OPTIONAL { ?contact a nco:PersonContact ; nco:nameFamily ?name}  \
			FILTER (?duration > 0) .} \
			ORDER BY desc(?date) LIMIT 1000"

		"""Querying the received calls"""
        	start=time.time()

		result=self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying received calls  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)

        def p_test_calls_04(self):

		query = "SELECT ?name ?date ?number ?duration \
			WHERE {?m a nmo:Call; \
			nmo:receivedDate ?date ; \
			nmo:duration ?duration; \
			nmo:from ?contact . \
			?contact a nco:PersonContact; \
			nco:hasPhoneNumber ?number . \
			OPTIONAL { ?contact a nco:PersonContact ; nco:nameFamily ?name}\
			FILTER (?duration > 0) .} \
			ORDER BY desc(?date) LIMIT 1000"


		"""Querying the missed calls"""
        	start=time.time()

		result=self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying missed calls  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)



""" IM performance  test cases """
class instant_messages(TestUpdate):


        def p_test_im_01(self):


		query = "SELECT ?message ?from ?date ?content WHERE { \
                ?message a nmo:IMMessage ; \
                nmo:from ?from ; \
                nmo:receivedDate ?date ;  \
                nie:plainTextContent ?content} LIMIT 10000"

		"""Querying the messages """
       		start=time.time()

		result=self.resources.SparqlQuery(query)

       		elapse =time.time()-start
       		print "Time taken for querying  messages  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)

        def p_test_im_02(self):

		query = "SELECT ?contact ?status WHERE{\
                   	?contact a  nco:IMAccount; \
                   	nco:imPresence ?status }LIMIT 10000"

		"""Querying the status of contacts every sec"""
        	start=time.time()

		result=self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying status of contacts = %s " %elapse
		print "no. of items retrieved: %d" %len(result)



""" rtcom performance  test cases """
class rtcom(TestUpdate):


        def p_test_rtcom_01(self):

		query = "SELECT ?channel ?participant nco:fullname(?participant) ?last_date nie:plainTextContent(?last_message) \
    				(SELECT COUNT(?message) AS ?message_count  \
					WHERE { ?message nmo:communicationChannel ?channel }) \
    				(SELECT COUNT(?message) AS ?message_count  \
					WHERE { ?message nmo:communicationChannel ?channel ; nmo:isRead true }) \
			WHERE { SELECT ?channel ?participant ?last_date  \
				(SELECT ?message WHERE { ?message nmo:communicationChannel ?channel ; nmo:sentDate ?date } ORDER BY DESC(?date) LIMIT 1) AS ?last_message \
    				WHERE { \
        			?channel a nmo:CommunicationChannel ; \
            			nmo:lastMessageDate ?last_date ; \
            			nmo:hasParticipant ?participant . \
        			FILTER (?participant != nco:default-contact-me ) \
    				} ORDER BY DESC(?last_date) LIMIT 50 }"


        	start=time.time()

		result=self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying conversation list view  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)


        def p_test_rtcom_02(self):

		query = "SELECT ?msg ?date ?text ?contact \
			WHERE { \
    			?msg nmo:communicationChannel <urn:channel:1268130692> ; \
        		nmo:sentDate ?date ; \
        		nie:plainTextContent ?text ; \
        		nmo:from [ nco:hasIMAddress ?fromAddress ] . \
    			<urn:channel:1268130692> nmo:hasParticipant ?contact . \
    			?contact nco:hasIMAddress ?fromAddress . \
			} ORDER BY DESC(?date) LIMIT 50"

		#query = "SELECT ?msg ?date ?text ?contact \
		#	WHERE { \
    		#	?msg nmo:communicationChannel <urn:uuid:7585395544138154780> ; \
        	#	nmo:sentDate ?date ; \
        	#	nie:plainTextContent ?text ; \
        	#	nmo:from [ nco:hasIMAddress ?fromAddress ] . \
    		#	<urn:uuid:7585395544138154780> nmo:hasParticipant ?contact . \
    		#	?contact nco:hasIMAddress ?fromAddress . \
		#	} ORDER BY DESC(?date) LIMIT 50"


        	start=time.time()

		result=self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying conversation view  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)



""" Audio, Video, Images  performance  test cases """
class audio(TestUpdate):


        def p_test_audio_01(self):

		""" Querying for Artist and finding the no.of albums in each artist.  """

		query = "SELECT ?artist ?name COUNT(DISTINCT ?album) COUNT (?song) \
                      WHERE { \
                      ?song a nmm:MusicPiece ; \
                      nmm:musicAlbum ?album;  \
                      nmm:performer ?artist . \
                      ?artist nmm:artistName ?name. \
                      } GROUP BY ?artist"

		start=time.time()

            	result=self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying Artist and finding the no.of albums in each artist  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)

        def p_test_audio_02(self):

                """Query all albums also count of songs in each album """

		query= "SELECT  ?album COUNT(?songs) AS ?count  WHERE { \
			?a a nmm:MusicAlbum; \
			nie:title ?album. \
			?mp nmm:musicAlbum ?a;\
			nie:title ?songs.\
                        }GROUP BY ?album ORDER BY DESC(?album)"

		start=time.time()

		result = self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying all albums and count their songs  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)

        def p_test_audio_03(self):

                """Query all songs """

		query = "SELECT DISTINCT ?title ?album ?artist \
			WHERE { { \
			?song a nmm:MusicPiece . \
			?song nie:title ?title .\
			?song nmm:performer ?perf . \
			?perf nmm:artistName ?artist .  \
			OPTIONAL{ ?song nmm:musicAlbum ?alb . \
			?alb nmm:albumTitle ?album .}}}  \
	    		ORDER BY ?title "

		start=time.time()

		result = self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying all songs  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)

	def p_test_audio_04 (self) :
                """Query all albums """

                query = "SELECT DISTINCT nmm:albumTitle(?album) AS ?Album  ?Artist  COUNT(?Songs)  AS ?Songs  ?album \
			WHERE { { ?Songs a nmm:MusicPiece .\
			?Songs nmm:musicAlbum ?album . \
			OPTIONAL{  \
			?Songs nmm:performer ?perf .\
			OPTIONAL{?perf nmm:artistName ?Artist .\
                        }}}}GROUP BY ?album ORDER BY ?album LIMIT 5000"

                start=time.time()

                result = self.resources.SparqlQuery(query)

                elapse =time.time()-start
                print "Time taken for querying 15000 albums  = %s " %elapse
                print "no. of items retrieved: %d" %len(result)

	def p_test_audio_05 (self):

                """ Query all artists """
                query = " SELECT nmm:artistName(?artist) AS ?artistTitle ?albumTitle COUNT(?album) AS ?album ?artist \
			WHERE {  \
			?song a nmm:MusicPiece  .\
			?song nmm:performer ?artist . \
			OPTIONAL  { ?song nmm:musicAlbum ?album .\
			OPTIONAL {?album nmm:albumTitle ?albumTitle .\
                        } } } GROUP BY ?artist  ORDER BY ?artist LIMIT 5000"

                start=time.time()
                print query
                result = self.resources.SparqlQuery(query,timeout= 600)

                elapse =time.time()-start
                print "Time taken for querying 5000 artists  = %s " %elapse
                print "no. of items retrieved: %d" %len(result)

	def p_test_audio_06 (self) :
                """Query 100 albums """

                query = "SELECT DISTINCT nmm:albumTitle(?album) AS ?Album  ?Artist  COUNT(?Songs)  AS ?Songs  ?album \
			WHERE { { ?Songs a nmm:MusicPiece .\
			?Songs nmm:musicAlbum ?album .\
			OPTIONAL{ \
			?Songs nmm:performer ?perf .\
			OPTIONAL{?perf nmm:artistName ?Artist .\
			}}}}GROUP BY ?album ORDER BY ?album LIMIT 100"

                start=time.time()

                result = self.resources.SparqlQuery(query)

                elapse =time.time()-start
                print "Time taken for querying 100 albums  = %s " %elapse
                print "no. of items retrieved: %d" %len(result)

	def p_test_audio_07 (self):

                """ Query 100 artists """

                query = "SELECT nmm:artistName(?artist) AS ?artistTitle ?albumTitle COUNT(?album) AS\
                           ?album ?artist \
			   WHERE {  \
			   ?song a nmm:MusicPiece  .\
			   ?song nmm:performer ?artist . \
			   OPTIONAL  { ?song nmm:musicAlbum ?album.\
                           OPTIONAL {?album nmm:albumTitle ?albumTitle .\
			   }}} GROUP BY ?artist  ORDER BY ?artist  LIMIT 100"""

                start=time.time()
                print query
                result = self.resources.SparqlQuery(query,timeout=600)

                elapse =time.time()-start
                print "Time taken for querying 100 artist  = %s " %elapse

        def p_test_audio_08(self):

                """Query all albums also count of songs in each album """
		"""simplified version of test_audio_02  """

		query= "SELECT nie:title(?a) COUNT(?songs) WHERE { \
			?a a nmm:MusicAlbum . \
			?mp nmm:musicAlbum ?a ; \
			nie:title ?songs . } \
			GROUP BY ?a ORDER BY DESC(nie:title(?a))"

		start=time.time()

		result = self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying all albums and count their songs  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)

	def p_test_audio_09 (self):

                """ Query all artists """
		"""simplified version of test_audio_05  """
		query = "SELECT nmm:artistName(?artist) nmm:albumTitle(?album) COUNT(?album) ?artist WHERE { \
				?song a nmm:MusicPiece . \
				?song nmm:performer ?artist . \
				OPTIONAL { ?song nmm:musicAlbum ?album . } } \
				GROUP BY ?artist ORDER BY ?artist LIMIT 5000"

                start=time.time()
                print query
                result = self.resources.SparqlQuery(query,timeout= 600)

                elapse =time.time()-start
                print "Time taken for querying 5000 artists  = %s " %elapse
                print "no. of items retrieved: %d" %len(result)

	def p_test_audio_10 (self):

                """ Query 100 artists """
		"""simplified version of test_audio_07  """

		query = "SELECT nmm:artistName(?artist) nmm:albumTitle(?album) COUNT(?album) ?artist WHERE { \
			?song a nmm:MusicPiece . \
			?song nmm:performer ?artist . \
			OPTIONAL  { ?song nmm:musicAlbum ?album . } } \
			GROUP BY ?artist ORDER BY ?artist LIMIT 100"

                start=time.time()
                print query
                result = self.resources.SparqlQuery(query,timeout=600)

                elapse =time.time()-start
                print "Time taken for querying 100 artist  = %s " %elapse


class gallery(TestUpdate):


        def p_test_gallery_01(self):

		""" Querying for all Images and Videos """

		query = "SELECT ?url ?filename ?modified ?_width ?_height ?is \
                    WHERE { \
                     ?media a nfo:Visual; \
                     nie:isStoredAs  ?is ;\
                     nie:url ?url;\
                     nfo:fileName ?filename ;\
                     nfo:fileLastModified ?modified .\
                     OPTIONAL    {?media nfo:width ?_width. } \
                     OPTIONAL   { ?media nfo:height ?_height .} } \
                     ORDER BY ?modified LIMIT 10000"

		start=time.time()

            	result=self.resources.SparqlQuery(query, timeout=25)

        	elapse =time.time()-start
        	print "Time taken for querying all images and videos  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)

        def p_test_gallery_02(self):

		""" Querying for all Images and Videos without OPTIONALS"""

		query = "SELECT ?url ?filename ?modified ?is \
                    WHERE { \
                     ?media a nfo:Visual; \
                     nie:isStoredAs  ?is ;\
                     nie:url ?url;\
                     nfo:fileName ?filename ;\
                     nfo:fileLastModified ?modified .}\
                     ORDER BY ?modified LIMIT 10000"

		start=time.time()

            	result=self.resources.SparqlQuery(query, timeout=25)

        	elapse =time.time()-start
        	print "Time taken for querying all images and videos without OPTIONALS  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)

        def p_test_gallery_03(self):

		""" Querying for 500 Images and Videos """

		query = "SELECT ?url ?filename ?modified ?_width ?_height ?is \
                    WHERE { \
                     ?media a nfo:Visual; \
                     nie:isStoredAs  ?is ;\
                     nie:url ?url;\
                     nfo:fileName ?filename ;\
                     nfo:fileLastModified ?modified .\
                     OPTIONAL    {?media nfo:width ?_width. } \
                     OPTIONAL   { ?media nfo:height ?_height .} } \
                     ORDER BY ?modified LIMIT 500"
		start=time.time()

            	result=self.resources.SparqlQuery(query, timeout=25)

        	elapse =time.time()-start
        	print "Time taken for querying 500 images and videos  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)


        def p_test_gallery_04(self):

		""" Querying for 500 Images and Videos without OPTIONALS"""

		query = "SELECT ?url ?filename ?modified ?is \
                    WHERE { \
                     ?media a nfo:Visual; \
                     nie:isStoredAs  ?is ;\
                     nie:url ?url;\
                     nfo:fileName ?filename ;\
                     nfo:fileLastModified ?modified .} \
                     ORDER BY ?modified LIMIT 500"

		start=time.time()

            	result=self.resources.SparqlQuery(query, timeout=25)

        	elapse =time.time()-start
        	print "Time taken for querying 100 images and videos without OPTIONALS  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)



        def p_test_gallery_05(self):

        	""" Querying for images, videos which have tag TEST """

		query  = "SELECT ?media \
                        WHERE { \
                     	?media a nfo:Visual; \
                        nao:hasTag ?tag . \
			?tag nao:prefLabel 'TEST' }"
		start=time.time()

            	result=self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying all images and videos with a tag  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)


        def p_test_gallery_06(self):

        	""" Querying for 500 images, videos which have tag TEST """
		query  = "SELECT ?media \
                        WHERE { \
                     	?media a nfo:Visual; \
                        nao:hasTag ?tag . \
			?tag nao:prefLabel 'TEST' } LIMIT 500"

		start=time.time()

            	result=self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying 500 images and videos with a tag  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)


        def p_test_gallery_07(self):

		"""Querying all images and videos taken with phone's camera """

		query = "SELECT ?media WHERE { \
                     	?media a nfo:Visual; \
                        nmm:camera 'NOKIA' }"

		start=time.time()

            	result=self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying all images and videos taken with phone's camera  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)


        def p_test_gallery_08(self):

		"""Querying 500 images and videos taken with phone's camera """

		query = "SELECT ?media WHERE { \
                     	?media a nfo:Visual; \
                        nmm:camera 'NOKIA' } LIMIT 500"

		start=time.time()

            	result=self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying 500 images and videos taken with phone's camera  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)

        def p_test_gallery_09(self):

		"""Querying all images """

		query = " SELECT ?url ?height ?width ?mime ?camera ?exposuretime ?fnumber ?focallength \
                        WHERE {\
			?image a nmm:Photo; \
                        nie:url ?url; \
			nie:mimeType ?mime. \
			OPTIONAL { ?image nfo:height ?height .}\
			OPTIONAL { ?image nfo:width  ?width .}\
			OPTIONAL { ?image nmm:camera ?camera .}\
			OPTIONAL { ?image nmm:exposureTime ?exposuretime .}\
			OPTIONAL { ?image nmm:fnumber ?fnumber .}\
			OPTIONAL { ?image nmm:focalLength ?focallength .}} LIMIT 10000"


		start=time.time()

            	result=self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying all images = %s " %elapse
		print "no. of items retrieved: %d" %len(result)



        def p_test_gallery_10(self):

		"""Querying 500 images """

		query = " SELECT ?url ?height ?width ?mime ?camera ?exposuretime ?fnumber ?focallength \
                        WHERE {\
			?image a nmm:Photo; \
                        nie:url ?url; \
			nie:mimeType ?mime. \
			OPTIONAL { ?image nfo:height ?height .}\
			OPTIONAL { ?image nfo:width  ?width .}\
			OPTIONAL { ?image nmm:camera ?camera .}\
			OPTIONAL { ?image nmm:exposureTime ?exposuretime .}\
			OPTIONAL { ?image nmm:fnumber ?fnumber .}\
			OPTIONAL { ?image nmm:focalLength ?focallength .}} LIMIT 500"


		start=time.time()

            	result=self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying 500 images = %s " %elapse
		print "no. of items retrieved: %d" %len(result)


        def p_test_gallery_11(self):

		""" Querying for 500 Images and Videos with UNION for them """

		query = "SELECT ?url ?filename ?modified ?_width ?_height ?is \
                    WHERE { \
                     {?media a nmm:Photo.} UNION {?media a nmm:Video.} \
                     ?media nie:isStoredAs  ?is .\
                     ?media nie:url ?url.\
                     ?media nfo:fileName ?filename .\
                     ?media nfo:fileLastModified ?modified .\
                     OPTIONAL    {?media nfo:width ?_width. } \
                     OPTIONAL   { ?media nfo:height ?_height .} } \
                     ORDER BY ?modified LIMIT 500"
		start=time.time()

            	result=self.resources.SparqlQuery(query, timeout=25)

        	elapse =time.time()-start
        	print "Time taken for querying 500 images and videos  = %s " %elapse
		print "no. of items retrieved: %d" %len(result)

        def p_test_gallery_12(self):

		"""Querying all images """
		"""simplified version of test_gallery_09 """

		query = "SELECT nie:url(?image) nfo:height(?image) nfo:width(?image) nie:mimeType(?image) nmm:camera(?image) nmm:exposureTime(?image) nmm:fnumber(?image) nmm:focalLength(?image) WHERE { ?image a nmm:Photo . } limit 10000"


		start=time.time()

            	result=self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying all images = %s " %elapse
		print "no. of items retrieved: %d" %len(result)

        def p_test_gallery_13(self):

		"""Querying 500 images """
		"""simplified version of test_gallery_10 """

		query = "SELECT nie:url(?image) nfo:height(?image) nfo:width(?image) nie:mimeType(?image) nmm:camera(?image) nmm:exposureTime(?image) nmm:fnumber(?image) nmm:focalLength(?image) WHERE { ?image a nmm:Photo . } limit 500"


		start=time.time()

            	result=self.resources.SparqlQuery(query)

        	elapse =time.time()-start
        	print "Time taken for querying 500 images = %s " %elapse
		print "no. of items retrieved: %d" %len(result)





class ftsmatch (TestUpdate) :

        def p_test_fts_01 (self):
            """Making a search for artist"""

            query = "  SELECT ?uri WHERE { \
                      ?uri a nie:InformationElement ; \
                      fts:match 'ArtistName' }"
            start=time.time()

            result=self.resources.SparqlQuery(query)

            elapse =time.time()-start
            print "Time taken for searching an artist in 10000 music files  " %elapse
            print "no. of items retrieved: %d" %len(result)


        def p_test_fts_02 (self) :
            """ Searching for a word """
            query = " SELECT ?uri WHERE { \
                     ?uri a nie:InformationElement ; \
		     fts:match 'WordInPlainText' . } "

            start=time.time()

            result=self.resources.SparqlQuery(query)

            elapse =time.time()-start
            print "Time taken for searching a word  = %s " %elapse
            print "no. of items retrieved: %d" %len(result)

	def p_test_fts_03 (self):
            """Making a search for artist"""

            query = "  SELECT ?uri WHERE { \
                      ?uri a nie:InformationElement ; \
                      fts:match 'ArtistNa*'}"
            start=time.time()

            result=self.resources.SparqlQuery(query)

            elapse =time.time()-start
            print "Time taken for searching an artist in 10000 music files  " %elapse
            print "no. of items retrieved: %d" %len(result)

        def p_test_fts_04 (self):
            """Making a search for artist"""

            query = "  SELECT ?uri WHERE { \
                      ?uri a nie:InformationElement ; \
                      fts:match 'Art*' }"
            start=time.time()

            result=self.resources.SparqlQuery(query)

            elapse =time.time()-start
            print "Time taken for searching an artist in 10000 music files  " %elapse
            print "no. of items retrieved: %d" %len(result)

	def p_test_fts_05 (self):
            """Making a search for artist"""

            query = "  SELECT ?uri WHERE { \
                      ?uri a nie:InformationElement ; \
                      fts:match 'Ar*'}"
            start=time.time()

            result=self.resources.SparqlQuery(query)

            elapse =time.time()-start
            print "Time taken for searching an artist in 10000 music files  " %elapse
            print "no. of items retrieved: %d" %len(result)


        def p_test_fts_06 (self):
            """Making a search for artist"""

            query = "  SELECT ?uri WHERE { \
                      ?uri a nie:InformationElement ; \
                      fts:match 'A*' }"
            start=time.time()

            result=self.resources.SparqlQuery(query)

            elapse =time.time()-start
            print "Time taken for searching an artist in 10000 music files  " %elapse
            print "no.of items retrieved: %d" %len(result)

	def p_test_fts_07 (self):

            """Making a search for artist"""

            query = "  SELECT ?uri WHERE { \
                      ?uri a nie:InformationElement ; \
                      fts:match 'A* p*' }"
            start=time.time()

            result=self.resources.SparqlQuery(query)

            elapse =time.time()-start
            print "Time taken for searching an artist in 10000 music files  " %elapse
            print "no. of items retrieved: %d" %len(result)

        def p_test_fts_08 (self):
            """Making a search for artist"""

            query = "  SELECT ?uri WHERE { \
                      ?uri a nie:InformationElement ; \
                      fts:match 'A* p* k*' }"
            start=time.time()

            result=self.resources.SparqlQuery(query)

            elapse =time.time()-start
            print "Time taken for searching an artist in 10000 music files %s " %elapse
            print "no. of items retrieved: %d" %len(result)




class content_manager (TestUpdate) :

        def p_test_cm_01 (self):


		"""Get all the contacts that match fts and get relevant UI info for them"""

		query = "SELECT DISTINCT ?url ?photourl ?imstatus tracker:coalesce(?family, ?given, ?orgname, ?nick, ?email, ?phone, ?blog) \
		WHERE { { ?url a nco:PersonContact.?url fts:match 'fami*'. } \
		UNION { ?url a nco:PersonContact. ?url nco:hasEmailAddress ?add.?add fts:match 'fami*'. } \
		UNION { ?url a nco:PersonContact. ?url nco:hasPostalAddress ?post.?post fts:match 'fami*'. } \
		OPTIONAL { ?url maemo:relevance ?relevance.} \
		OPTIONAL { ?url nco:photo ?photo. ?photo nie:url ?photourl.} \
		OPTIONAL { ?url nco:imContactStatusMessage ?imstatus.} \
		OPTIONAL { ?url nco:nameFamily ?family.} \
		OPTIONAL { ?url nco:nameFamily ?given.} \
		OPTIONAL { ?url nco:org ?org. ?org nco:nameGiven ?orgname.} \
		OPTIONAL { ?url nco:hasIMAccount ?acc. ?acc nco:imNickname ?nick.} \
		OPTIONAL { ?url nco:hasEmailAddress ?hasemail. ?hasemail nco:emailAddress ?email.} \
		OPTIONAL { ?url nco:hasPhoneNumber ?hasphone. ?hasphone nco:phoneNumber ?phone.} \
		OPTIONAL { ?url nco:blogUrl ?blog.}} \
		ORDER BY ?relevance \
		LIMIT 100"


		start=time.time()

		result=self.resources.SparqlQuery(query)

		elapse =time.time()-start
		print "Time taken to get 100 contacts that match fts and get relevant UI info for them %s " %elapse
		print "no. of items retrieved: %d" %len(result)


        def p_test_cm_02 (self):


		"""Get all the contacts that match fts and get relevant UI info for them"""

		query = "SELECT DISTINCT ?url tracker:coalesce(nco:nameFamily(?url), nco:nameGiven(?url), 'unknown') \
		WHERE { \
		{ ?url a nco:PersonContact.?url fts:match 'fami*'. } \
		UNION { ?url a nco:PersonContact. ?url nco:hasEmailAddress ?add.?add fts:match 'fami*'. } \
		UNION { ?url a nco:PersonContact. ?url nco:hasPostalAddress ?post.?post fts:match 'fami*'. } \
		} LIMIT 100"


		start=time.time()

		result=self.resources.SparqlQuery(query)

		elapse =time.time()-start
		print "Time taken to get 100 contacts that match fts and get relevant UI info for them %s " %elapse
		print "no. of items retrieved: %d" %len(result)


        def p_test_cm_03 (self):


		"""Get all the messages """

		query = "SELECT DISTINCT ?url nie:title(?url) \
		WHERE { \
		{ ?url a nmo:Message. ?url fts:match 'fami*'. } \
		UNION { ?url a nmo:Message. ?url nmo:from ?from . ?from fts:match 'fami*'. } \
		UNION { ?url a nmo:Message. ?url nmo:recipient ?to . ?to fts:match 'fami*'. } \
		} LIMIT 100"


		start=time.time()

		result=self.resources.SparqlQuery(query)

		elapse =time.time()-start
		print "Time taken to get 100 contacts that match fts and get relevant UI info for them %s " %elapse
		print "no. of items retrieved: %d" %len(result)


        def p_test_cm_04 (self):

		"""Get all the messages """

		query = "SELECT ?url ?fileLastModified ?relevance ?fileName ?mimeType ?url2 \
			WHERE { \
			?url a nfo:Image .\
			?url nfo:fileLastModified ?fileLastModified. \
			?url nfo:fileName ?fileName .\
			?url nie:mimeType ?mimeType .\
			?url nie:url ?url2 . \
			OPTIONAL { ?url maemo:relevance ?relevance. } \
			} ORDER BY ?_fileName"


		start=time.time()

		result=self.resources.SparqlQuery(query)

		elapse =time.time()-start
		print "Time taken to get 100 contacts that match fts and get relevant UI info for them %s " %elapse
		print "no. of items retrieved: %d" %len(result)







if __name__ == "__main__":
        unittest.main()

