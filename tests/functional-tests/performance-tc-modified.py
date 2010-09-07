#!/usr/bin/env python
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

import sys,os
import dbus
import commands, signal
import unittest
import time
import random
import datetime
import htmldoc as hd

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

	def parsing(self, method, query):
		f=open("./report/"+method.__name__+"_query.html", "w")
		f.write(query)
		f.close()
		value=hd.parseDoc(method)
		l=hd.Link(method.__name__+"_query", "report/"+method.__name__+"_query.html")
		return value, l


""" email performance test cases """
class email(TestUpdate):


        def test_email_01(self):

		"""
	        Time taken for querying the emails.	
		@param description: Time taken for querying the emails 
		@param expected_value: 
		@param querytype: 
		"""

		query = "SELECT ?m ?From  ?date ?email1 WHERE { \
                 	?m a  nmo:Email ; \
                 	nmo:receivedDate ?date ;\
                 	nmo:from ?From . ?from nco:hasEmailAddress ?email1 } LIMIT 10000"

		list=[]
		for i in range ( 0, iterations ):
			"""Query for emails """
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse

			print "Time taken for querying emails = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(email.test_email_01, query)
		t.rows.append(hd.TableRow([email.test_email_01.__name__, value[0], value[1], list, value[2], len(result), l]))



""" calls performance  test cases """
class calls(TestUpdate):


        def test_calls_01(self):

		"""
	        Time taken for querying duration of calls from phonenumbers.	
		@param description: Time taken for querying duration of calls from phonenumbers
		@param expected_value: 
		@param querytype: 
		"""

		query = "SELECT ?duration ?phonenumber WHERE {\
                   	?call  a  nmo:Call ;\
                   	nmo:duration ?duration ;\
                   	nmo:from [a nco:Contact ; nco:hasPhoneNumber ?phonenumber] }LIMIT 10000"
		list=[]
		for i in range ( 0, iterations ):

			"""Querying the duration of calls of contacts """
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying duration of calls from phonenumbers  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)
			
		value, l=self.parsing(calls.test_calls_01, query)
		t.rows.append(hd.TableRow([calls.test_calls_01.__name__, value[0], value[1], list, value[2], len(result), l]))

        def test_calls_02(self):

		"""
	        Time taken for querying dialed calls.	
		@param description: Time taken for querying dialed calls 
		@param expected_value: 
		@param querytype: 
		"""

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

		list=[]
		for i in range ( 0, iterations ):
			"""Querying the dialed calls"""
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying dialed calls  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(calls.test_calls_02, query)
		t.rows.append(hd.TableRow([calls.test_calls_02.__name__, value[0], value[1], list, value[2], len(result), l]))


        def test_calls_03(self):

		"""
	        Time taken for querying received calls
		@param description: Time taken for querying received calls
		@param expected_value: 
		@param querytype: 
		"""

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

		list=[]
		for i in range ( 0, iterations ):
			"""Querying the received calls"""
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying received calls  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(calls.test_calls_03, query)
		t.rows.append(hd.TableRow([calls.test_calls_03.__name__, value[0], value[1], list, value[2], len(result), l]))

        def test_calls_04(self):

		"""
	        Time taken for querying missed calls.	
		@param description: Time taken for querying missed calls
		@param expected_value: 
		@param querytype: 
		"""

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


		list=[]
		for i in range ( 0, iterations ):
			"""Querying the missed calls"""
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying missed calls  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(calls.test_calls_04, query)
		t.rows.append(hd.TableRow([calls.test_calls_04.__name__, value[0], value[1], list, value[2], len(result), l]))



""" IM performance  test cases """
class instant_messages(TestUpdate):


        def test_im_01(self):

		"""
	        Time taken for querying  messages.	
		@param description: Time taken for querying  messages 
		@param expected_value: 
		@param querytype: 
		"""


		query = "SELECT ?message ?from ?date ?content WHERE { \
			?message a nmo:IMMessage ; \
			nmo:from ?from ; \
			nmo:receivedDate ?date ;  \
			nie:plainTextContent ?content} LIMIT 10000"

		list=[]
		for i in range ( 0, iterations ):
			"""Querying the messages """
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying  messages  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(instant_messages.test_im_01, query)
		t.rows.append(hd.TableRow([instant_messages.test_im_01.__name__, value[0], value[1], list, value[2], len(result), l]))

        def test_im_02(self):

		"""
	        Time taken for querying status of contacts.	
		@param description: Time taken for querying status of contacts
		@param expected_value: 
		@param querytype: 
		"""

		query = "SELECT ?contact ?status WHERE{\
                   	?contact a  nco:IMAccount; \
                   	nco:imPresence ?status }LIMIT 10000"

		list=[]
		for i in range ( 0, iterations ):
			"""Querying the status of contacts every sec"""
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying status of contacts = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(instant_messages.test_im_02, query)
		t.rows.append(hd.TableRow([instant_messages.test_im_02.__name__, value[0], value[1], list, value[2], len(result), l]))



""" rtcom performance  test cases """
class rtcom(TestUpdate):


        def test_rtcom_01(self):

		"""
	        Time taken for querying (old) conversation view (without contact info).	
		@param description: Time taken for querying (old) conversation view (without contact info)
		@param expected_value: 0.5 sec
		@param querytype: 
		"""

		# A version of the next one that skips the contact parts that are not generated properly

		query = "SELECT ?msg ?date ?text ?contact \
			WHERE { \
    			?msg nmo:communicationChannel <urn:channel:1> ; \
        		nmo:sentDate ?date ; \
        		nie:plainTextContent ?text . \
    			<urn:channel:1> nmo:hasParticipant ?contact . \
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


		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying (old) conversation view (without contact info)  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(rtcom.test_rtcom_01, query)
		t.rows.append(hd.TableRow([rtcom.test_rtcom_01.__name__, value[0], value[1], list, value[2], len(result), l], bgcolor="#C3FDB8"))

        def test_rtcom_02(self):

		"""
	        Time taken for querying conversation view.	
		@param description: Time taken for querying conversation view
		@param expected_value: 0.5 sec
		@param querytype: 
		"""

		#
		# Current rtcom queries, please do not "quietly optimize".
		#
		
		# requires secondary index support to be fast

		query = " \
			SELECT ?message ?date ?from ?to \
			     rdf:type(?message) \
			     tracker:coalesce(fn:concat(nco:nameGiven(?contact), ' ', nco:nameFamily(?contact)), nco:nickname(?contact)) \
			     nco:contactUID(?contact) \
			     nmo:communicationChannel(?message) \
			     nmo:isSent(?message) \
			     nmo:isDraft(?message) \
			     nmo:isRead(?message) \
			     nmo:isAnswered(?message) \
			     nmo:isDeleted(?message) \
			     nmo:messageId(?message) \
			     nmo:smsId(?message) \
			     nmo:sentDate(?message) \
			     nmo:receivedDate(?message) \
			     nie:contentLastModified(?message) \
			     nmo:messageSubject(?message) \
			     nie:plainTextContent(?message) \
			     nmo:deliveryStatus(?message) \
			     nmo:reportDelivery(?message) \
			     nie:url(?message) \
			     nfo:fileName(nmo:fromVCard(?message)) \
			     rdfs:label(nmo:fromVCard(?message)) \
			     nfo:fileName(nmo:toVCard(?message)) \
			     rdfs:label(nmo:toVCard(?message)) \
			     nmo:encoding(?message) \
			     nie:characterSet(?message) \
			     nie:language(?message) \
			WHERE \
			{ \
			  SELECT \
			     ?message ?date ?from ?to \
			     (SELECT ?contact \
			      WHERE \
			      { \
				  { \
				    <urn:channel:1> nmo:hasParticipant ?participant . \
				    ?contact a nco:PersonContact . \
				    ?participant nco:hasIMAddress ?imaddress . \
				    ?contact nco:hasIMAddress ?imaddress . \
				  } \
				  UNION \
				  { \
				    <urn:channel:1> nmo:hasParticipant ?participant . \
				    ?contact a nco:PersonContact . \
				    ?participant nco:hasPhoneNumber ?participantNumber . \
				    ?participantNumber maemo:localPhoneNumber ?number . \
				    ?contact nco:hasPhoneNumber ?contactNumber . \
				    ?contactNumber maemo:localPhoneNumber ?number . \
				  } \
			      } \
			     ) AS ?contact \
			  WHERE \
			  { \
			    ?message a nmo:Message . \
			    ?message nmo:isDraft false . \
			    ?message nmo:isDeleted false . \
			    ?message nmo:sentDate ?date . \
			    ?message nmo:from ?fromContact . \
			    ?message nmo:to ?toContact . \
			    ?fromContact nco:hasContactMedium ?from . \
			    ?toContact nco:hasContactMedium ?to . \
			    ?message nmo:communicationChannel <urn:channel:1> . \
			  } \
			  ORDER BY DESC(?date) \
			} \
			LIMIT 50 \
			"

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying conversation view  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(rtcom.test_rtcom_02, query)
		t.rows.append(hd.TableRow([rtcom.test_rtcom_02.__name__, value[0], value[1], list, value[2], len(result), l], bgcolor="#C3FDB8"))

        def test_rtcom_03(self):

		"""
	        Time taken for querying call history.
		@param description: Time taken for querying call history
		@param expected_value: 0.5 sec
		@param querytype: 
		"""

		#
		# Current rtcom queries, please do not "quietly optimize".
		#

		query = " \
			SELECT ?call ?date ?from ?to \
			     rdf:type(?call) \
			     nmo:isSent(?call) \
			     nmo:isAnswered(?call) \
			     nmo:isRead(?call) \
			     nmo:sentDate(?call) \
			     nmo:receivedDate(?call) \
			     nmo:duration(?call) \
			     nie:contentLastModified(?call) \
			     (SELECT ?contact \
			      WHERE \
			      { \
				  { \
				      ?contact a nco:PersonContact . \
				      ?contact nco:contactUID ?contactId . \
				      { \
					?call nmo:to ?address . \
				      } \
				      UNION \
				      { \
					?call nmo:from ?address . \
				      } \
				    ?address nco:hasIMAddress ?imaddress . \
				    ?contact nco:hasIMAddress ?imaddress . \
				  } \
				  UNION \
				  { \
				      ?contact a nco:PersonContact . \
				      ?contact nco:contactUID ?contactId . \
				      { \
					?call nmo:to ?address . \
				      } \
				      UNION \
				      { \
					?call nmo:from ?address . \
				      } \
				    ?address nco:hasPhoneNumber ?addressNumber . \
				    ?addressNumber maemo:localPhoneNumber ?number . \
				    ?contact nco:hasPhoneNumber ?contactNumber . \
				    ?contactNumber maemo:localPhoneNumber ?number . \
				  } \
			      }) \
			WHERE \
			{ \
			  { \
			    ?call a nmo:Call . \
			    ?call nmo:sentDate ?date . \
			    ?call nmo:from ?fromContact . \
			    ?call nmo:to ?toContact . \
			    ?fromContact nco:hasContactMedium ?from . \
			    ?toContact nco:hasContactMedium ?to . \
			  } \
			} \
			ORDER BY DESC(?date) LIMIT 50\
			"

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying call history  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(rtcom.test_rtcom_03, query)
		t.rows.append(hd.TableRow([rtcom.test_rtcom_03.__name__, value[0], value[1], list, value[2], len(result), l], bgcolor="#C3FDB8"))

        def test_rtcom_04(self):

		"""
	        Time taken for querying (old) conversation list view.	
		@param description: Time taken for querying (old) conversation list view 
		@param expected_value: 
		@param querytype: 
		"""

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


		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying (old) conversation list view  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(rtcom.test_rtcom_04, query)
		t.rows.append(hd.TableRow([rtcom.test_rtcom_04.__name__, value[0], value[1], list, value[2], len(result), l]))


        def test_rtcom_05(self):

		"""
	        Time taken for querying (old) conversation view.	
		@param description: Time taken for querying (old) conversation view
		@param expected_value: 
		@param querytype: 
		"""

		query = "SELECT ?msg ?date ?text ?contact \
			WHERE { \
			?msg nmo:communicationChannel <urn:channel:1> ; \
        		nmo:sentDate ?date ; \
        		nie:plainTextContent ?text ; \
        		nmo:from [ nco:hasIMAddress ?fromAddress ] . \
    			<urn:channel:1> nmo:hasParticipant ?contact . \
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


		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying (old) conversation view  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(rtcom.test_rtcom_05, query)
		t.rows.append(hd.TableRow([rtcom.test_rtcom_05.__name__, value[0], value[1], list, value[2], len(result), l]))

        def test_rtcom_06(self):

		"""
	        Time taken for querying conversation list.	
		@param description: Time taken for querying conversation list
		@param expected_value: 
		@param querytype: 
		"""

		#
		# Current rtcom queries, please do not "quietly optimize".
		#

		query = " \
			SELECT ?channel ?subject nie:generator(?channel) \
			  tracker:coalesce(fn:concat(nco:nameGiven(?contact), ' ', nco:nameFamily(?contact)), nco:nickname(?contact)) AS ?contactName \
			  nco:contactUID(?contact) AS ?contactUID \
			  ?lastDate ?lastMessage nie:plainTextContent(?lastMessage) \
			  nfo:fileName(nmo:fromVCard(?lastMessage)) \
			  rdfs:label(nmo:fromVCard(?lastMessage)) \
			  ( SELECT COUNT(?message) AS ?total_messages WHERE { ?message nmo:communicationChannel ?channel . }) \
			  ( SELECT COUNT(?message) AS ?total_unread   WHERE { ?message nmo:communicationChannel ?channel . ?message nmo:isRead false  .}) \
			  ( SELECT COUNT(?message) AS ?_total_sent    WHERE { ?message nmo:communicationChannel ?channel . ?message nmo:isSent true . }) \
			WHERE { \
			  SELECT ?channel  ?subject  ?lastDate \
				  \
				 ( SELECT ?message WHERE {?message nmo:communicationChannel ?channel . ?message nmo:sentDate ?sentDate .} ORDER BY DESC(?sentDate) LIMIT 1) AS ?lastMessage \
			      (SELECT ?contact \
			      WHERE { \
				    { \
				      ?channel nmo:hasParticipant ?participant . \
				      ?contact a nco:PersonContact . \
				      ?participant nco:hasIMAddress ?imaddress . \
				      ?contact nco:hasIMAddress ?imaddress . \
				    } \
				    UNION \
				    { \
				      ?channel nmo:hasParticipant ?participant . \
				      ?contact a nco:PersonContact . \
				      ?participant nco:hasPhoneNumber ?participantNumber . \
				      ?number maemo:localPhoneNumber ?localNumber . \
				      ?contact nco:hasPhoneNumber ?contactNumber . \
				      ?contactNumber maemo:localPhoneNumber ?localNumber . \
				    } \
				}) AS ?contact \
			  WHERE { \
			      ?channel a nmo:CommunicationChannel . \
			      ?channel nie:subject ?subject . \
			      ?channel nmo:lastMessageDate ?lastDate . \
			    } \
			} \
			ORDER BY DESC(?lastDate) LIMIT 50\
			"


		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying conversation list  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(rtcom.test_rtcom_06, query)
		t.rows.append(hd.TableRow([rtcom.test_rtcom_06.__name__, value[0], value[1], list, value[2], len(result), l]))


""" Audio, Video, Images  performance  test cases """
class audio(TestUpdate):

        def test_audio_01(self):

		"""
	        Time taken for querying all songs.	
		@param description: Time taken for querying all songs
		@param expected_value: First 20 clips in 0.2 sec and the rest up to 10000 in 2 sec 
		@param querytype: 
		"""

                """Query all songs """

		query = "SELECT ?title ?artist nmm:albumTitle (nmm:musicAlbum (?song))\
			WHERE { { \
			?song a nmm:MusicPiece . \
			?song nie:title ?title .\
			?song nmm:performer ?perf . \
			?perf nmm:artistName ?artist .}}  \
	    		ORDER BY ?title "

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result = self.resources.SparqlQuery(query,timeout = 1000)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying all songs  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(audio.test_audio_01, query)
		t.rows.append(hd.TableRow([audio.test_audio_01.__name__, value[0], value[1], list, value[2], len(result), l], bgcolor="#C3FDB8"))

	def test_audio_02 (self) :

		"""
	        Time taken for querying 5000 albums.
		@param description: Time taken for querying 5000 albums
		@param expected_value: First 20 albums in 0.2sec rest up to 1000 in 2 sec 
		@param querytype: 
		"""

                """Query all albums """

                query = "SELECT nmm:albumTitle(?album) AS ?Album  nmm:artistName (nmm:performer (?Songs)) COUNT(?Songs)  AS ?Songs  ?album \
			WHERE { { ?Songs a nmm:MusicPiece .\
			?Songs nmm:musicAlbum ?album . \
                        }}GROUP BY ?album ORDER BY ?album LIMIT 5000"

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result = self.resources.SparqlQuery(query,timeout = 1000)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying 5000 albums  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(audio.test_audio_02, query)
		t.rows.append(hd.TableRow([audio.test_audio_02.__name__, value[0], value[1], list, value[2], len(result), l], bgcolor="#C3FDB8"))

	def test_audio_03 (self):

		"""
	        Time taken for querying 5000 artists.	
		@param description: Time taken for querying 5000 artists 
		@param expected_value: First 20 artists in 0.2 secrst up to 100 in 0.5sec
		@param querytype: 
		"""

                """ Query all artists """
		"""simplified version of test_audio_08  """
		query = "SELECT nmm:artistName(?artist) nmm:albumTitle(?album) COUNT(?album) ?artist WHERE { \
				?song a nmm:MusicPiece . \
				?song nmm:performer ?artist . \
				OPTIONAL { ?song nmm:musicAlbum ?album . } } \
				GROUP BY ?artist ORDER BY ?artist LIMIT 5000"

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()
			print query
			result = self.resources.SparqlQuery(query,timeout= 600)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying 5000 artists  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(audio.test_audio_03, query)
		t.rows.append(hd.TableRow([audio.test_audio_03.__name__, value[0], value[1], list, value[2], len(result), l], bgcolor="#C3FDB8"))


        def test_audio_04(self):

		"""
	        Time taken for querying Artist and finding the no.of albums in each artist.
		@param description: Time taken for querying Artist and finding the no.of albums in each artist
		@param expected_value: First 20 albums in 0.2sec and the rest up to 100 in 0.5 sec 
		@param querytype: 
		"""

		""" Querying for Artist and finding the no.of albums in each artist.  """

		query = "SELECT ?artist ?name COUNT(?album) COUNT (?song) \
                      WHERE { \
                      ?song a nmm:MusicPiece ; \
                      nmm:musicAlbum ?album;  \
                      nmm:performer ?artist . \
                      ?artist nmm:artistName ?name. \
                      } GROUP BY ?artist"

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying Artist and finding the no.of albums in each artist  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(audio.test_audio_04, query)
		t.rows.append(hd.TableRow([audio.test_audio_04.__name__, value[0], value[1], list, value[2], len(result), l], bgcolor="#C3FDB8"))

        def test_audio_05(self):

		"""
	        Time taken for querying all artists and count their songs.
		@param description: Time taken for querying all artists and count their songs
		@param expected_value: First 20 clips in 0.2 sec and the rest up to 1000 in 2sec
		@param querytype: 
		"""

                """Query all aritists also count of songs in each artists """

		query= "SELECT nmm:artistName(?artist) COUNT(?songs) WHERE { \
			?mp a nmm:MusicPiece . \
			?mp nmm:performer ?artist ; \
			nie:title ?songs . } \
			GROUP BY ?artist ORDER BY DESC(nmm:artistName(?artist))"

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result = self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying all artists and count their songs  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(audio.test_audio_05, query)
		t.rows.append(hd.TableRow([audio.test_audio_05.__name__, value[0], value[1], list, value[2], len(result), l], bgcolor="#C3FDB8"))

        def test_audio_06(self):

		"""
	        Time taken for querying all albums and count their songs.
		@param description: Time taken for querying all albums and count their songs
		@param expected_value: First 20 clips in 0.2 sec 
		@param querytype: 
		"""

                """Query all albums also count of songs in each album """

		query= "SELECT nie:title(?a) COUNT(?songs) WHERE { \
			?a a nmm:MusicAlbum . \
			?mp nmm:musicAlbum ?a ; \
			nie:title ?songs . } \
			GROUP BY ?a ORDER BY DESC(nie:title(?a))"

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result = self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying all albums and count their songs  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(audio.test_audio_06, query)
		t.rows.append(hd.TableRow([audio.test_audio_06.__name__, value[0], value[1], list, value[2], len(result), l], bgcolor="#C3FDB8"))

        def test_audio_07(self):

		"""
	        Time taken for querying all albums and count their songs.	
		@param description: Time taken for querying all albums and count their songs 
		@param expected_value: 
		@param querytype: 
		"""

                """Query all albums also count of songs in each album """

		query= "SELECT  ?album COUNT(?songs) AS ?count  WHERE { \
			?a a nmm:MusicAlbum; \
			nie:title ?album. \
			?mp nmm:musicAlbum ?a;\
			nie:title ?songs.\
                        }GROUP BY ?album ORDER BY DESC(?album)"

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result = self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying all albums and count their songs  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(audio.test_audio_07, query)
		t.rows.append(hd.TableRow([audio.test_audio_07.__name__, value[0], value[1], list, value[2], len(result), l]))


	def test_audio_08 (self):

		"""
	        Time taken for querying 5000 artists.	
		@param description: Time taken for querying 5000 artists 
		@param expected_value: 1.5
		@param querytype: 
		"""

                """ Query all artists """
                query = " SELECT nmm:artistName(?artist) AS ?artistTitle (nmm:musicAlbum (?song)) (nmm:albumTitle (?album))COUNT(?album) AS ?album ?artist \
			WHERE {  \
			?song a nmm:MusicPiece  .\
			?song nmm:performer ?artist . \
                        } GROUP BY ?artist  ORDER BY ?artist LIMIT 5000"

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()
			print query
			result = self.resources.SparqlQuery(query,timeout= 600)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying 5000 artists  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(audio.test_audio_08, query)
		t.rows.append(hd.TableRow([audio.test_audio_08.__name__, value[0], value[1], list, value[2], len(result), l]))

	def test_audio_09 (self) :

		"""
	        Time taken for querying 100 albums.	
		@param description: Time taken for querying 100 albums 
		@param expected_value: 
		@param querytype: 
		"""

                """Query 100 albums """

                query = "SELECT nmm:albumTitle(?album) AS ?Album  (nmm:performer(?Songs)) nmm:artistName COUNT(?Songs)  AS ?Songs  ?album \
			WHERE { { ?Songs a nmm:MusicPiece .\
			?Songs nmm:musicAlbum ?album .\
			}}GROUP BY ?album ORDER BY ?album LIMIT 100"

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result = self.resources.SparqlQuery(query,timeout = 1000)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying 100 albums  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(audio.test_audio_09, query)
		t.rows.append(hd.TableRow([audio.test_audio_09.__name__, value[0], value[1], list, value[2], len(result), l]))

	def test_audio_10 (self):

		"""
	        Time taken for querying 100 artist.
		@param description: Time taken for querying 100 artist
		@param expected_value: 
		@param querytype: 
		"""

                """ Query 100 artists """

                query = "SELECT nmm:artistName(?artist) AS ?artistTitle (nmm:musicAlbum (?song)) nmm:albumTitle COUNT(?album) AS\
                           ?album ?artist \
			   WHERE {  \
			   ?song a nmm:MusicPiece  .\
			   ?song nmm:performer ?artist . \
			   } GROUP BY ?artist  ORDER BY ?artist  LIMIT 100"""

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()
			print query
			result = self.resources.SparqlQuery(query,timeout=600)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying 100 artist  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(audio.test_audio_10, query)
		t.rows.append(hd.TableRow([audio.test_audio_10.__name__, value[0], value[1], list, value[2], len(result), l]))



	def test_audio_11 (self):

		"""
	        Time taken for querying 100 artist.	
		@param description: Time taken for querying 100 artist
		@param expected_value: 
		@param querytype: 
		"""

                """ Query 100 artists """
		"""simplified version of test_audio_10  """

		query = "SELECT nmm:artistName(?artist) nmm:albumTitle(?album) COUNT(?album) ?artist WHERE { \
			?song a nmm:MusicPiece . \
			?song nmm:performer ?artist . \
			OPTIONAL  { ?song nmm:musicAlbum ?album . } } \
			GROUP BY ?artist ORDER BY ?artist LIMIT 100"

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()
			print query
			result = self.resources.SparqlQuery(query,timeout=600)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying 100 artist  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)
		value, l=self.parsing(audio.test_audio_11, query)
		t.rows.append(hd.TableRow([audio.test_audio_11.__name__, value[0], value[1], list, value[2], len(result), l]))


class gallery(TestUpdate):


        def test_gallery_01(self):

		"""
	        Time taken for querying all images and videos.	
		@param description: Time taken for querying all images and videos 
		@param expected_value: Use streaming mode, get first 500 results in 0.1 second, all results in 2 second.
		@param querytype: 
		"""

		""" Querying for all Images and Videos """

		query = "SELECT ?url \
                    WHERE { \
                     ?media a nfo:Visual; \
                     nie:url ?url;\
                     nfo:fileName ?filename ;\
                     nfo:fileLastModified ?modified .\
                     OPTIONAL    {?media nfo:width ?_width. } \
                     OPTIONAL   { ?media nfo:height ?_height .} } \
                     ORDER BY ?modified LIMIT 10000"

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query, timeout=25)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying all images and videos  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(gallery.test_gallery_01, query)
		t.rows.append(hd.TableRow([gallery.test_gallery_01.__name__, value[0], value[1], list, value[2], len(result), l], bgcolor="#C3FDB8"))
		

        def test_gallery_02(self):

		"""
	        Time taken for querying all images and videos taken with phone's camera.
		@param description: Time taken for querying all images and videos taken with phone's camera
		@param expected_value: Use streaming mode, get first 500 results in 0.1 second, all results in 1.0 second.
		@param querytype: 
		"""

		"""Querying all images and videos taken with phone's camera """

		query = "SELECT ?media WHERE { \
                        ?media a nfo:Visual; \
                        nfo:equipment [ a nfo:Equipment; nfo:make 'NOKIA' ] }"

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying all images and videos taken with phone's camera  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(gallery.test_gallery_02, query)
		t.rows.append(hd.TableRow([gallery.test_gallery_02.__name__, value[0], value[1], list, value[2], len(result), l], bgcolor="#C3FDB8"))

        def test_gallery_03(self):

		"""
	        Time taken for querying all images and videos with a tag.
		@param description: Time taken for querying all images and videos with a tag
		@param expected_value: Use streaming mode, get first 500 results in 0.1 second, all results in 2 second.
		@param querytype: 
		"""

        	""" Querying for images, videos which have tag TEST """

		query  = "SELECT ?media \
                        WHERE { \
                     	?media a nfo:Visual; \
                        nao:hasTag ?tag . \
			?tag nao:prefLabel 'TEST' }"
		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying all images and videos with a tag  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(gallery.test_gallery_03, query)
		t.rows.append(hd.TableRow([gallery.test_gallery_03.__name__, value[0], value[1], list, value[2], len(result), l], bgcolor="#C3FDB8"))


        def test_gallery_04(self):

		"""
	        Time taken for querying all images and videos without OPTIONALS.	
		@param description: Time taken for querying all images and videos without OPTIONALS
		@param expected_value: 
		@param querytype: 
		"""

		""" Querying for all Images and Videos without OPTIONALS"""

		query = "SELECT ?url \
                    WHERE { \
                     ?media a nfo:Visual; \
                     nie:url ?url;\
                     nfo:fileName ?filename ;\
                     nfo:fileLastModified ?modified .}\
                     ORDER BY ?modified LIMIT 10000"

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query, timeout=25)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying all images and videos without OPTIONALS  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(gallery.test_gallery_04, query)
		t.rows.append(hd.TableRow([gallery.test_gallery_04.__name__, value[0], value[1], list, value[2], len(result), l]))


        def test_gallery_05(self):

		"""
	        Time taken for querying 500 images and videos.	
		@param description: Time taken for querying 500 images and videos
		@param expected_value: 
		@param querytype: 
		"""

		""" Querying for 500 Images and Videos """

		query = "SELECT ?url \
                    WHERE { \
                     ?media a nfo:Visual; \
                     nie:url ?url;\
                     nfo:fileName ?filename ;\
                     nfo:fileLastModified ?modified .\
                     OPTIONAL    {?media nfo:width ?_width. } \
                     OPTIONAL   { ?media nfo:height ?_height .} } \
                     ORDER BY ?modified LIMIT 500"
		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query, timeout=25)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying 500 images and videos  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(gallery.test_gallery_05, query)
		t.rows.append(hd.TableRow([gallery.test_gallery_05.__name__, value[0], value[1], list, value[2], len(result), l]))



        def test_gallery_06(self):

		"""
	        Time taken for querying 100 images and videos without OPTIONALS.
		@param description: Time taken for querying 100 images and videos without OPTIONALS
		@param expected_value: 
		@param querytype: 
		"""

		""" Querying for 500 Images and Videos without OPTIONALS"""

		query = "SELECT ?url \
                    WHERE { \
                     ?media a nfo:Visual; \
                     nie:url ?url;\
                     nfo:fileName ?filename ;\
                     nfo:fileLastModified ?modified .} \
                     ORDER BY ?modified LIMIT 500"

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query, timeout=25)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying 100 images and videos without OPTIONALS  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(gallery.test_gallery_06, query)
		t.rows.append(hd.TableRow([gallery.test_gallery_06.__name__, value[0], value[1], list, value[2], len(result), l]))



        def test_gallery_07(self):

		"""
	        Time taken for querying 500 images and videos with a tag.	
		@param description: Time taken for querying 500 images and videos with a tag
		@param expected_value: Use streaming mode, get first 500 results in 0.1 second, all results in 2 second.
		@param querytype: 
		"""

        	""" Querying for 500 images, videos which have tag TEST """
		query  = "SELECT ?media \
                        WHERE { \
                     	?media a nfo:Visual; \
                        nao:hasTag ?tag . \
			?tag nao:prefLabel 'TEST' } LIMIT 500"

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying 500 images and videos with a tag  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(gallery.test_gallery_07, query)
		t.rows.append(hd.TableRow([gallery.test_gallery_07.__name__, value[0], value[1], list, value[2], len(result), l]))



        def test_gallery_08(self):

		"""
	        Time taken for querying 500 images and videos taken with phone's camera.	
		@param description: Time taken for querying 500 images and videos taken with phone's camera
		@param expected_value: Use streaming mode, get first 500 results in 0.1 second, all results in 1.0 second.
		@param querytype: 
		"""

		"""Querying 500 images and videos taken with phone's camera """

		query = "SELECT ?media WHERE { \
                        ?media a nfo:Visual; \
                        nfo:equipment [ a nfo:Equipment; nfo:make 'NOKIA' ] } LIMIT 500"

		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying 500 images and videos taken with phone's camera  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(gallery.test_gallery_08, query)
		t.rows.append(hd.TableRow([gallery.test_gallery_08.__name__, value[0], value[1], list, value[2], len(result), l]))


        def test_gallery_09(self):

		"""
	        Time taken for querying all images.	
		@param description: Time taken for querying all images
		@param expected_value: Use streaming mode, get first 500 results in 0.1 second, all results in 1.0 second.
		@param querytype: 
		"""

		"""Querying all images """

		query = " SELECT ?url ?mime ?camera ?exposuretime ?fnumber ?focallength \
                        WHERE {\
			?image a nmm:Photo; \
                        nie:url ?url; \
			nie:mimeType ?mime. \
			OPTIONAL { ?image nfo:height ?height .}\
			OPTIONAL { ?image nfo:width  ?width .}\
			OPTIONAL { ?image nfo:equipment [ nfo:model ?camera ] .}\
			OPTIONAL { ?image nmm:exposureTime ?exposuretime .}\
			OPTIONAL { ?image nmm:fnumber ?fnumber .}\
			OPTIONAL { ?image nmm:focalLength ?focallength .}} LIMIT 10000"


		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying all images = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(gallery.test_gallery_09, query)
		t.rows.append(hd.TableRow([gallery.test_gallery_09.__name__, value[0], value[1], list, value[2], len(result), l]))




        def test_gallery_10(self):

		"""
	        Time taken for querying 500 images.	
		@param description: Time taken for querying 500 images
		@param expected_value: 
		@param querytype: 
		"""

		"""Querying 500 images """

		query = " SELECT ?url ?mime ?camera ?exposuretime ?fnumber ?focallength \
                        WHERE {\
			?image a nmm:Photo; \
                        nie:url ?url; \
			nie:mimeType ?mime. \
			OPTIONAL { ?image nfo:height ?height .}\
			OPTIONAL { ?image nfo:width  ?width .}\
			OPTIONAL { ?image nfo:equipment [ nfo:model ?camera ] .}\
			OPTIONAL { ?image nmm:exposureTime ?exposuretime .}\
			OPTIONAL { ?image nmm:fnumber ?fnumber .}\
			OPTIONAL { ?image nmm:focalLength ?focallength .}} LIMIT 500"


		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying 500 images = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(gallery.test_gallery_10, query)
		t.rows.append(hd.TableRow([gallery.test_gallery_10.__name__, value[0], value[1], list, value[2], len(result), l]))



        def test_gallery_11(self):

		"""
	        Time taken for querying 500 images and videos.	
		@param description: Time taken for querying 500 images and videos
		@param expected_value: 
		@param querytype: 
		"""

		""" Querying for 500 Images and Videos with UNION for them """

		query = "SELECT ?url \
                    WHERE { \
                     {?media a nmm:Photo.} UNION {?media a nmm:Video.} \
                     ?media nie:url ?url.\
                     ?media nfo:fileName ?filename .\
                     ?media nfo:fileLastModified ?modified .\
                     OPTIONAL    {?media nfo:width ?_width. } \
                     OPTIONAL   { ?media nfo:height ?_height .} } \
                     ORDER BY ?modified LIMIT 500"
		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query,timeout=1000)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying 500 images and videos  = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(gallery.test_gallery_11, query)
		t.rows.append(hd.TableRow([gallery.test_gallery_11.__name__, value[0], value[1], list, value[2], len(result), l]))


        def test_gallery_12(self):

		"""
	        Time taken for querying all images.	
		@param description: TTime taken for querying all images
		@param expected_value: 
		@param querytype: 
		"""

		"""Querying all images """
		"""simplified version of test_gallery_09 """

		query = "SELECT nie:url(?image) nie:mimeType(?image) nfo:model (nfo:equipment (?image)) nmm:exposureTime(?image) nmm:fnumber(?image) nmm:focalLength(?image) WHERE { ?image a nmm:Photo . } limit 10000"


		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying all images = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(gallery.test_gallery_12, query)
		t.rows.append(hd.TableRow([gallery.test_gallery_12.__name__, value[0], value[1], list, value[2], len(result), l]))


        def test_gallery_13(self):

		"""
	        Time taken for querying 500 images.	
		@param description: Time taken for querying 500 images
		@param expected_value: 
		@param querytype: 
		"""

		"""Querying 500 images """
		"""simplified version of test_gallery_10 """

		query = "SELECT nie:url(?image) nie:mimeType(?image) nfo:model (nfo:equipment (?image)) nmm:exposureTime(?image) nmm:fnumber(?image) nmm:focalLength(?image) WHERE { ?image a nmm:Photo . } limit 500"


		list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken for querying 500 images = %s " %elapse
			print "no. of items retrieved: %d" %len(result)
			list.append(elapse)

		value, l=self.parsing(gallery.test_gallery_13, query)
		t.rows.append(hd.TableRow([gallery.test_gallery_13.__name__, value[0], value[1], list, value[2], len(result), l]))



class ftsmatch (TestUpdate) :

        def test_fts_01 (self):

            """
	    Time taken for searching an artist in 10000 music files.	
	    @param description: Time taken for searching an artist in 10000 music files
	    @param expected_value: 
		@param querytype: 
	    """

            query = "  SELECT ?uri WHERE { \
                      ?uri a nie:InformationElement ; \
                      fts:match 'ArtistName' }"
	    list=[]
	    for i in range ( 0, iterations ):
		    start=time.time()

		    result=self.resources.SparqlQuery(query)

		    elapse =time.time()-start
		    elapse = "%.3f" %elapse 
		    print "Time taken for searching an artist in 10000 music files  %s" %elapse
		    print "no. of items retrieved: %d" %len(result)
		    list.append(elapse)

            value, l=self.parsing(ftsmatch.test_fts_01, query)
	    t.rows.append(hd.TableRow([ftsmatch.test_fts_01.__name__, value[0], value[1], list, value[2], len(result), l]))


        def test_fts_02 (self) :

            """
            Time taken for searching a word.	
	    @param description: Time taken for searching a word 
	    @param expected_value: 
		@param querytype: 
	    """

            """ Searching for a word """
            query = " SELECT ?uri WHERE { \
                     ?uri a nie:InformationElement ; \
		     fts:match 'WordInPlainText' . } "

	    list=[]
	    for i in range ( 0, iterations ):
		    start=time.time()

		    result=self.resources.SparqlQuery(query)

		    elapse =time.time()-start
                    elapse = "%.3f" %elapse 
		    print "Time taken for searching a word  = %s " %elapse
		    print "no. of items retrieved: %d" %len(result)
		    list.append(elapse)

            value, l=self.parsing(ftsmatch.test_fts_02, query)
	    t.rows.append(hd.TableRow([ftsmatch.test_fts_02.__name__, value[0], value[1], list, value[2], len(result), l]))

	def test_fts_03 (self):

            """
            Time taken for searching an artist in 10000 music files.	
	    @param description: Time taken for searching an artist in 10000 music files
	    @param expected_value: 
		@param querytype: 
	    """

            """Making a search for artist"""

            query = "  SELECT ?uri WHERE { \
                      ?uri a nie:InformationElement ; \
                      fts:match 'ArtistNa*'}"
	    list=[]
	    for i in range ( 0, iterations ):
		    start=time.time()

		    result=self.resources.SparqlQuery(query)

		    elapse =time.time()-start
		    elapse = "%.3f" %elapse 
		    print "Time taken for searching an artist in 10000 music files %s " %elapse
		    print "no. of items retrieved: %d" %len(result)
		    list.append(elapse)

            value, l=self.parsing(ftsmatch.test_fts_03, query)
	    t.rows.append(hd.TableRow([ftsmatch.test_fts_03.__name__, value[0], value[1], list, value[2], len(result), l]))

        def test_fts_04 (self):

            """
	    Time taken for searching an artist in 10000 music files.	
	    @param description: Time taken for searching an artist in 10000 music files
	    @param expected_value: 
		@param querytype: 
	    """

            """Making a search for artist"""

            query = "  SELECT ?uri WHERE { \
                      ?uri a nie:InformationElement ; \
                      fts:match 'Art*' }"
	    list=[]
	    for i in range ( 0, iterations ):
		    start=time.time()

		    result=self.resources.SparqlQuery(query)

		    elapse =time.time()-start
		    elapse = "%.3f" %elapse 
		    print "Time taken for searching an artist in 10000 music files  %s" %elapse
		    print "no. of items retrieved: %d" %len(result)
		    list.append(elapse)

            value, l=self.parsing(ftsmatch.test_fts_04, query)
	    t.rows.append(hd.TableRow([ftsmatch.test_fts_04.__name__, value[0], value[1], list, value[2], len(result), l]))

	def test_fts_05 (self):

            """
            Time taken for searching an artist in 10000 music files.	
	    @param description: Time taken for searching an artist in 10000 music files
	    @param expected_value: 
		@param querytype: 
	    """

            """Making a search for artist"""

            query = "  SELECT ?uri WHERE { \
                      ?uri a nie:InformationElement ; \
                      fts:match 'Ar*'}"
	    list=[]
	    for i in range ( 0, iterations ):
		    start=time.time()

		    result=self.resources.SparqlQuery(query)

		    elapse =time.time()-start
		    elapse = "%.3f" %elapse 
		    print "Time taken for searching an artist in 10000 music files  %s" %elapse
		    print "no. of items retrieved: %d" %len(result)
		    list.append(elapse)

            value, l=self.parsing(ftsmatch.test_fts_05, query)
	    t.rows.append(hd.TableRow([ftsmatch.test_fts_05.__name__, value[0], value[1], list, value[2], len(result), l]))


        def test_fts_06 (self):

            """
            Time taken for searching an artist in 10000 music files.	
	    @param description: Time taken for searching an artist in 10000 music files
	    @param expected_value: 
		@param querytype: 
	    """

            """Making a search for artist"""

            query = "  SELECT ?uri WHERE { \
                      ?uri a nie:InformationElement ; \
                      fts:match 'A*' }"
	    list=[]
            for i in range ( 0, iterations ):
		    start=time.time()

		    result=self.resources.SparqlQuery(query)

		    elapse =time.time()-start
		    elapse = "%.3f" %elapse 
		    print "Time taken for searching an artist in 10000 music files  %s" %elapse
		    print "no.of items retrieved: %d" %len(result)
		    list.append(elapse)

            value, l=self.parsing(ftsmatch.test_fts_06, query)
	    t.rows.append(hd.TableRow([ftsmatch.test_fts_06.__name__, value[0], value[1], list, value[2], len(result), l]))

	def test_fts_07 (self):

            """
            Time taken for searching an artist in 10000 music files.
	    @param description: Time taken for searching an artist in 10000 music files
	    @param expected_value: 
		@param querytype: 
	    """


            """Making a search for artist"""

            query = "  SELECT ?uri WHERE { \
                      ?uri a nie:InformationElement ; \
                      fts:match 'A* p*' }"
	    list=[]
            for i in range ( 0, iterations ):
		    start=time.time()

		    result=self.resources.SparqlQuery(query)

		    elapse =time.time()-start
		    elapse = "%.3f" %elapse 
		    print "Time taken for searching an artist in 10000 music files  %s" %elapse
		    print "no. of items retrieved: %d" %len(result)
		    list.append(elapse)

            value, l=self.parsing(ftsmatch.test_fts_07, query)
	    t.rows.append(hd.TableRow([ftsmatch.test_fts_07.__name__, value[0], value[1], list, value[2], len(result), l]))

        def test_fts_08 (self):

            """
            Time taken for searching an artist in 10000 music files.	
	    @param description: Time taken for searching an artist in 10000 music files
	    @param expected_value: 
		@param querytype: 
	    """

            """Making a search for artist"""

            query = "  SELECT ?uri WHERE { \
                      ?uri a nie:InformationElement ; \
                      fts:match 'A* p* k*' }"
	    list=[]
            for i in range ( 0, iterations ):
		    start=time.time()

		    result=self.resources.SparqlQuery(query)

		    elapse =time.time()-start
		    elapse = "%.3f" %elapse 
		    print "Time taken for searching an artist in 10000 music files %s " %elapse
		    print "no. of items retrieved: %d" %len(result)
		    list.append(elapse)

            value, l=self.parsing(ftsmatch.test_fts_08, query)
	    t.rows.append(hd.TableRow([ftsmatch.test_fts_08.__name__, value[0], value[1], list, value[2], len(result), l]))




class content_manager (TestUpdate) :

        def test_cm_01 (self):

		"""
	        Time taken to get 100 contacts that match fts and get relevant UI info for them.	
		@param description: Time taken to get 100 contacts that match fts and get relevant UI info for them
		@param expected_value: 
		@param querytype: 
		"""


		"""Get all the contacts that match fts and get relevant UI info for them"""

		query = "SELECT ?url ?photourl ?imstatus tracker:coalesce(?family, ?given, ?orgname, ?nick, ?email, ?phone, ?blog) \
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


	        list=[]
                for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 100 contacts that match fts and get relevant UI info for them %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(content_manager.test_cm_01, query)
		t.rows.append(hd.TableRow([content_manager.test_cm_01.__name__, value[0], value[1], list, value[2], len(result), l]))


        def test_cm_02 (self):

		"""
	        Time taken to get 100 contacts that match fts and get relevant UI info for them.	
		@param description: Time taken to get 100 contacts that match fts and get relevant UI info for them
		@param expected_value: 6.13
		@param querytype: 
		"""


		"""Get all the contacts that match fts and get relevant UI info for them"""

		query = "SELECT ?url tracker:coalesce(nco:nameFamily(?url), nco:nameGiven(?url), 'unknown') \
			WHERE { \
			{ ?url a nco:PersonContact.?url fts:match 'fami*'. } \
			UNION { ?url a nco:PersonContact. ?url nco:hasEmailAddress ?add.?add fts:match 'fami*'. } \
			UNION { ?url a nco:PersonContact. ?url nco:hasPostalAddress ?post.?post fts:match 'fami*'. } \
			} LIMIT 100"


	        list=[]
                for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 100 contacts that match fts and get relevant UI info for them %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(content_manager.test_cm_02, query)
		t.rows.append(hd.TableRow([content_manager.test_cm_02.__name__, value[0], value[1], list, value[2], len(result), l]))


        def test_cm_03 (self):

		"""
	        Time taken to get 100 contacts that match fts and get relevant UI info for them.	
		@param description: Time taken to get 100 contacts that match fts and get relevant UI info for them 
		@param expected_value: 
		@param querytype: 
		"""


		"""Get all the messages """

		query = "SELECT ?url nie:title(?url) \
			WHERE { \
			{ ?url a nmo:Message. ?url fts:match 'fami*'. } \
			UNION { ?url a nmo:Message. ?url nmo:from ?from . ?from fts:match 'fami*'. } \
			UNION { ?url a nmo:Message. ?url nmo:recipient ?to . ?to fts:match 'fami*'. } \
			} LIMIT 100"


	        list=[]
                for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 100 contacts that match fts and get relevant UI info for them %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(content_manager.test_cm_03, query)
		t.rows.append(hd.TableRow([content_manager.test_cm_03.__name__, value[0], value[1], list, value[2], len(result), l]))


        def test_cm_04 (self):

		"""
	        Time taken to get 100 contacts that match fts and get relevant UI info for them.	
		@param description: Time taken to get 100 contacts that match fts and get relevant UI info for them
		@param expected_value: 8.77
		@param querytype: 
		"""

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

	        list=[]
		for i in range ( 0, iterations ):
			
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 100 contacts that match fts and get relevant UI info for them %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(content_manager.test_cm_04, query)
		t.rows.append(hd.TableRow([content_manager.test_cm_04.__name__, value[0], value[1], list, value[2], len(result), l]))


        def test_cm_05 (self):

		"""
	        Time taken to get 100 content items that match fts without UI info for them.	
		@param description: Time taken to get 100 content items that match fts without UI info for them
		@param expected_value: 
		@param querytype: 
		"""

		"""Get all the matching data """

		query = "SELECT ?glob_url \
		        WHERE \
			{ \
			  { SELECT ?url as ?glob_url \
			    WHERE { ?url a nmo:Message . \
				     ?url fts:match 'fami*' . \
				     ?url nmo:from ?from . } } \
			  UNION \
			  { SELECT ?url as ?glob_url \
			    WHERE { ?url a nmo:Message . \
                                     ?url nmo:from ?from . \
                                     ?from fts:match 'fami*'. } } \
			  UNION \
			  { SELECT ?url as ?glob_url \
			    WHERE { ?url a nmo:Message . \
				     ?url nmo:to ?to . \
				     ?to fts:match 'fami*' . } } \
			  UNION \
			  { SELECT ?url as ?glob_url \
			    WHERE { ?url a nmo:Message. \
				     ?url nmo:communicationChannel ?cha . \
				     ?cha fts:match 'fami*'. } } \
			  UNION \
			  { SELECT ?url as ?glob_url \
			    WHERE { ?url a nco:PersonContact . \
				     ?url fts:match 'fami*'. } } \
			  UNION \
			  { SELECT ?url as ?glob_url \
			    WHERE { ?url a nco:PersonContact . \
				     ?url nco:hasEmailAddress ?email . \
				     ?email fts:match 'fami*'. } } \
			  UNION \
			  { SELECT ?url as ?glob_url \
			    WHERE { ?url a nco:PersonContact . \
				     ?url nco:hasPostalAddress ?post . \
				     ?post fts:match 'fami*'. } } \
			  UNION \
			  { SELECT ?url as ?glob_url \
			    WHERE { ?url a nmm:MusicPiece . \
				     ?url nmm:performer ?artist . \
				     ?artist fts:match 'fami*' . } } \
			  UNION \
			  { SELECT ?url as ?glob_url \
			    WHERE { ?url a nmm:MusicPiece . \
				     ?url nmm:musicAlbum ?album . \
				     ?album fts:match 'fami*' . } } \
			  } \
			LIMIT 100"

	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 100 content items that match fts without UI info for them %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(content_manager.test_cm_05, query)
		t.rows.append(hd.TableRow([content_manager.test_cm_05.__name__, value[0], value[1], list, value[2], len(result), l]))


	def test_cm_06 (self):

		"""
	        Time taken to get 100 content items that match fts and get relevant UI info for them.
		@param description: Time taken to get 100 content items that match fts and get relevant UI info for them
		@param expected_value: 
		@param querytype: 
		"""

		"""Get all the matching data """

		query = "SELECT ?glob_url ?first ?second \
		        WHERE \
			{ \
			  { SELECT ?url as ?glob_url \
			    nmo:messageSubject(?url) as ?first \
			    tracker:coalesce(nco:fullname(?from), nco:nameGiven(?from), nco:nameFamily(?from), nco:org(?from),'unknown') as ?second \
			    WHERE { ?url a nmo:Message . \
				     ?url fts:match 'fami*' . \
				     ?url nmo:from ?from . } } \
			  UNION \
			  { SELECT ?url as ?glob_url \
			    nmo:messageSubject(?url) as ?first \
			    tracker:coalesce(nco:fullname(?from), nco:nameGiven(?from), nco:nameFamily(?from), nco:org(?from),'unknown') as ?second \
			    WHERE { ?url a nmo:Message . \
                                     ?url nmo:from ?from . \
                                     ?from fts:match 'fami*'. } } \
			  UNION \
			  { SELECT ?url as ?glob_url \
			    nmo:messageSubject(?url) as ?first \
			    tracker:coalesce(nco:fullname(?from), nco:nameGiven(?from), nco:nameFamily(?from), nco:org(?from),'unknown') as ?second \
			    WHERE { ?url a nmo:Message . \
				     ?url nmo:to ?to . \
				     ?to fts:match 'fami*' . \
				     ?url nmo:from ?from . } } \
			  UNION \
			  { SELECT ?url as ?glob_url \
			    nmo:messageSubject(?url) as ?first \
			    tracker:coalesce(nco:fullname(?from), nco:nameGiven(?from), nco:nameFamily(?from), nco:org(?from),'unknown') as ?second \
			    WHERE { ?url a nmo:Message. \
				     ?url nmo:communicationChannel ?cha . \
				     ?cha fts:match 'fami*'. \
				     ?url nmo:from ?from . } } \
			  UNION \
			  { SELECT ?url as ?glob_url \
			    tracker:coalesce(nco:fullname(?url), nco:nameGiven(?url), nco:nameFamily(?url), nco:org(?url),'unknown') as ?first \
			    tracker:coalesce(nco:emailAddress(?email), nco:imNickname(?im), 'unknown') as ?second \
			    WHERE { ?url a nco:PersonContact . \
				     ?url fts:match 'fami*'. \
				     { SELECT ?em as ?email WHERE { ?url nco:hasEmailAddress ?em }  LIMIT 1 } \
				     { SELECT ?imadd as ?im WHERE { ?url nco:hasIMAddress ?imadd }  LIMIT 1 } } } \
			  UNION \
			  { SELECT ?url as ?glob_url \
			    tracker:coalesce(nco:fullname(?url), nco:nameGiven(?url), nco:nameFamily(?url), nco:org(?url),'unknown') as ?first \
			    tracker:coalesce(nco:emailAddress(?email), nco:imNickname(?im), 'unknown') as ?second \
			    WHERE { ?url a nco:PersonContact . \
				     ?url nco:hasEmailAddress ?email . \
				     ?email fts:match 'fami*'. \
				     { SELECT ?imadd as ?im WHERE { ?url nco:hasIMAddress ?imadd }  LIMIT 1 } } } \
			  UNION \
			  { SELECT ?url as ?glob_url \
			    tracker:coalesce(nco:fullname(?url), nco:nameGiven(?url), nco:nameFamily(?url), nco:org(?url),'unknown') as ?first \
			    tracker:coalesce(nco:emailAddress(?email), nco:imNickname(?im), 'unknown') as ?second \
			    WHERE { ?url a nco:PersonContact . \
				     ?url nco:hasPostalAddress ?post . \
				     ?post fts:match 'fami*'. \
				     { SELECT ?em as ?email WHERE { ?url nco:hasEmailAddress ?em }  LIMIT 1 } \
				     { SELECT ?imadd as ?im WHERE { ?url nco:hasIMAddress ?imadd }  LIMIT 1 } } } \
			  UNION \
			  { SELECT ?url as ?glob_url \
			    nie:title(?url) as ?first \
			    fn:concat(nmm:artistName(?artist),'-',nmm:albumTitle(?album)) \
			    WHERE { ?url a nmm:MusicPiece . \
				     ?url nmm:performer ?artist . \
				     ?artist fts:match 'fami*' . \
				     OPTIONAL { ?url nmm:musicAlbum ?album . } } } \
			  UNION \
			  { SELECT ?url as ?glob_url \
			    nie:title(?url) as ?first \
			    fn:concat(nmm:artistName(?artist),'-',nmm:albumTitle(?album)) \
			    WHERE { ?url a nmm:MusicPiece . \
				     ?url nmm:musicAlbum ?album . \
				     ?album fts:match 'fami*' . \
				     OPTIONAL { ?url nmm:performer ?artist }} } \
			  } \
			LIMIT 100"

	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 100 content items that match fts and get relevant UI info for them %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(content_manager.test_cm_06, query)
		t.rows.append(hd.TableRow([content_manager.test_cm_06.__name__, value[0], value[1], list, value[2], len(result), l]))


class contacts (TestUpdate) :

        def test_contacts_01 (self):

		"""
	        Time taken to get 50 contacts basic information (original).
		@param description: Time taken to get 50 contacts basic information (original)(Bug : 176170) 
		@param expected_value: 
		@param querytype: Original
		"""

		query = " \
			SELECT \
			  ?_contact \
			  ?_Avatar_ImageUrl \
			  ?_Birthday_Birthday \
			  bound(?_Gender_Gender) AS ?_Gender_Gender_IsBound \
			  (?_Gender_Gender = nco:gender-female) AS ?_Gender_Gender_IsEqual_Female \
			  (?_Gender_Gender = nco:gender-male) AS ?_Gender_Gender_IsEqual_Male \
			  ?_Guid_Guid \
			  ?_Name_Prefix \
			  ?_Name_FirstName \
			  ?_Name_MiddleName \
			  ?_Name_LastName \
			  ?_Name_Suffix \
			  ?_Nickname_Nickname \
			  ?_Note_Note \
			  ?_Timestamp_CreationTimestamp \
			  ?_Timestamp_ModificationTimestamp \
			WHERE \
			{ \
			  { \
			    ?_contact rdf:type nco:PersonContact . \
			    OPTIONAL { ?_contact nco:photo ?__1 . ?__1 nfo:fileUrl ?_Avatar_ImageUrl . } \
			    OPTIONAL { ?_contact nco:birthDate ?_Birthday_Birthday . } \
			    OPTIONAL { ?_contact nco:gender ?_Gender_Gender . } \
			    OPTIONAL { ?_contact nco:contactUID ?_Guid_Guid . } \
			    OPTIONAL { ?_contact nco:nameHonorificPrefix ?_Name_Prefix . } \
			    OPTIONAL { ?_contact nco:nameGiven ?_Name_FirstName . } \
			    OPTIONAL { ?_contact nco:nameAdditional ?_Name_MiddleName . } \
			    OPTIONAL { ?_contact nco:nameFamily ?_Name_LastName . } \
			    OPTIONAL { ?_contact nco:nameHonorificSuffix ?_Name_Suffix . } \
			    OPTIONAL { ?_contact nco:nickname ?_Nickname_Nickname . } \
			    OPTIONAL { ?_contact nco:note ?_Note_Note . } \
			    OPTIONAL { ?_contact nie:contentCreated ?_Timestamp_CreationTimestamp . } \
			    OPTIONAL { ?_contact nie:contentLastModified ?_Timestamp_ModificationTimestamp . } \
			  } \
			} \
			ORDER BY ?_contact LIMIT 50 \
			"

	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 50 contacts basic information (original) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(contacts.test_contacts_01, query)
		t.rows.append(hd.TableRow([contacts.test_contacts_01.__name__, value[0], value[1], list, value[2], len(result), l]))


	def test_contacts_02 (self):

		"""
	        Time taken to get 50 contacts basic information (modified)
		@param description: Time taken to get 50 contacts basic information (modified) 
		@param expected_value: 
		@param querytype: Modified
		"""

		query = " \
			SELECT \
			  ?_contact \
			  ?_Avatar_ImageUrl \
			  nco:birthDate(?_contact) \
			  bound(?_Gender_Gender) \
			  (?_Gender_Gender = nco:gender-female) \
			  (?_Gender_Gender = nco:gender-male) \
			  nco:contactUID(?_contact) \
			  nco:nameHonorificPrefix(?_contact) \
			  nco:nameGiven(?_contact) \
			  nco:nameAdditional(?_contact) \
			  nco:nameFamily(?_contact) \
			  nco:nameHonorificSuffix(?_contact) \
			  nco:nickname(?_contact) \
			  nco:note(?_contact) \
			  nie:contentCreated(?_contact) \
			  nie:contentLastModified(?_contact) \
			WHERE \
			{ \
			  { \
			    ?_contact rdf:type nco:PersonContact . \
			    OPTIONAL { ?_contact nco:photo ?__1 . ?__1 nfo:fileUrl ?_Avatar_ImageUrl . } \
			    OPTIONAL { ?_contact nco:gender ?_Gender_Gender . } \
			  } \
			} \
			ORDER BY ?_contact LIMIT 50 \
			"

	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 50 contacts basic information (modified) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(contacts.test_contacts_02, query)
		t.rows.append(hd.TableRow([contacts.test_contacts_02.__name__, value[0], value[1], list, value[2], len(result), l]))


	def test_contacts_03 (self):

		"""
	        Time taken to get 50 contacts address information (original)
		@param description: Time taken to get 50 contacts address information (original) 
		@param expected_value: 
		@param querytype: Original
		"""

		query = " \
			SELECT  \
			  ?_contact \
			  ?_Address_Country \
			  ?_Address_Locality \
			  ?_Address_PostOfficeBox \
			  ?_Address_Postcode \
			  ?_Address_Region \
			  ?_Address_Street \
			  bound(?_Address_SubTypes_Domestic) AS ?_Address_SubTypes_Domestic_IsBound \
			  bound(?_Address_SubTypes_International) AS ?_Address_SubTypes_International_IsBound \
			  bound(?_Address_SubTypes_Parcel) AS ?_Address_SubTypes_Parcel_IsBound \
			  bound(?_Address_SubTypes_Postal) AS ?_Address_SubTypes_Postal_IsBound \
			  bound(?_Address_Context_Work) AS ?_Address_Context_Work_IsBound \
			WHERE \
			{ \
			  { \
			    ?_contact rdf:type nco:PersonContact . \
			    { \
			      ?_contact nco:hasPostalAddress ?__1 . \
			      ?__1 nco:country ?_Address_Country . \
			      ?__1 nco:locality ?_Address_Locality . \
			      ?__1 nco:pobox ?_Address_PostOfficeBox . \
			      ?__1 nco:postalcode ?_Address_Postcode . \
			      ?__1 nco:region ?_Address_Region . \
			      ?__1 nco:streetAddress ?_Address_Street . \
			      OPTIONAL { \
				?__1 rdf:type ?_Address_SubTypes_Domestic . \
				FILTER((?_Address_SubTypes_Domestic = nco:DomesticDeliveryAddress)) . \
			      } \
			      OPTIONAL { \
				?__1 rdf:type ?_Address_SubTypes_International . \
				FILTER((?_Address_SubTypes_International = nco:InternationalDeliveryAddress)) . \
			      } \
			      OPTIONAL { \
				?__1 rdf:type ?_Address_SubTypes_Parcel . \
				FILTER((?_Address_SubTypes_Parcel = nco:ParcelDeliveryAddress)) . \
			      } \
			      OPTIONAL { \
				?__1 rdf:type ?_Address_SubTypes_Postal . \
				FILTER((?_Address_SubTypes_Postal = nco:PostalAddress)) . \
			      } \
			    } \
			    UNION \
			    { \
			      ?_contact nco:hasAffiliation ?_Address_Context_Work . \
			      ?_Address_Context_Work nco:hasPostalAddress ?__2 . \
			      ?__2 nco:country ?_Address_Country . \
			      ?__2 nco:locality ?_Address_Locality . \
			      ?__2 nco:pobox ?_Address_PostOfficeBox . \
			      ?__2 nco:postalcode ?_Address_Postcode . \
			      ?__2 nco:region ?_Address_Region . \
			      ?__2 nco:streetAddress ?_Address_Street . \
			      OPTIONAL { \
				?__2 rdf:type ?_Address_SubTypes_Domestic . \
				FILTER((?_Address_SubTypes_Domestic = nco:DomesticDeliveryAddress)) . \
			      } \
			      OPTIONAL { \
				?__2 rdf:type ?_Address_SubTypes_International . \
				FILTER((?_Address_SubTypes_International = nco:InternationalDeliveryAddress)) . \
			      } \
			      OPTIONAL { \
				?__2 rdf:type ?_Address_SubTypes_Parcel . \
				FILTER((?_Address_SubTypes_Parcel = nco:ParcelDeliveryAddress)) . \
			      } \
			      OPTIONAL { \
				?__2 rdf:type ?_Address_SubTypes_Postal . \
				FILTER((?_Address_SubTypes_Postal = nco:PostalAddress)) . \
			      } \
			    } \
			  } \
			} \
			ORDER BY ?_contact LIMIT 50 \
			"

	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 50 contacts address information (original) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(contacts.test_contacts_03, query)
		t.rows.append(hd.TableRow([contacts.test_contacts_03.__name__, value[0], value[1], list, value[2], len(result), l]))


	def test_contacts_04 (self):

		"""
	        Time taken to get 50 contacts address information (modified)
		@param description: Time taken to get 50 contacts address information (modified)
		@param expected_value: 
		@param querytype: Modified
		"""

		query = " \
			SELECT \
			  ?contact \
			  nco:country(?postal) \
			  nco:locality(?postal) \
			  nco:pobox(?postal) \
			  nco:postalcode(?postal) \
			  nco:region(?postal) \
			  nco:streetAddress(?postal) \
			  bound(?work) \
			WHERE \
			{ \
			  ?contact rdf:type nco:PersonContact . \
			  { ?contact nco:hasPostalAddress ?postal . } \
			  UNION \
			  { ?contact nco:hasAffiliation ?work . \
			      ?work nco:hasPostalAddress ?postal . \
			  } \
			} \
			ORDER BY ?contact LIMIT 50 \
			"

	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 50 contacts address information (modified) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(contacts.test_contacts_04, query)
		t.rows.append(hd.TableRow([contacts.test_contacts_04.__name__, value[0], value[1], list, value[2], len(result), l]))

	def test_contacts_05 (self):

		"""
	        Time taken to get 50 contacts email information (original)
		@param description: Time taken to get 50 contacts email information (original)
		@param expected_value: 
		@param querytype: Original
		"""


		query = " \
			SELECT \
			  ?_contact ?_EmailAddress ?_EmailAddress_EmailAddress \
			  bound(?_EmailAddress_Context_Work) AS ?_EmailAddress_Context_Work_IsBound \
			WHERE \
			{ \
			  { \
			    ?_contact rdf:type nco:PersonContact . \
			    { \
			      ?_contact nco:hasEmailAddress ?_EmailAddress . \
			      ?_EmailAddress nco:emailAddress ?_EmailAddress_EmailAddress . \
			    } \
			    UNION \
			    { \
			      ?_contact nco:hasAffiliation ?_EmailAddress_Context_Work . \
			      ?_EmailAddress_Context_Work nco:hasEmailAddress ?_EmailAddress . \
			      ?_EmailAddress nco:emailAddress ?_EmailAddress_EmailAddress . \
			    } \
			  } \
			} \
			ORDER BY ?_contact LIMIT 50 \
			"

	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 50 contacts email information (original) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(contacts.test_contacts_05, query)
		t.rows.append(hd.TableRow([contacts.test_contacts_05.__name__, value[0], value[1], list, value[2], len(result), l]))

	def test_contacts_06 (self):

		"""
	        Time taken to get 50 contacts email information (modified).
		@param description: Time taken to get 50 contacts email information (modified)
		@param expected_value: 
		@param querytype: Modified
		"""

		query = " \
			SELECT \
			  ?contact \
			  ?email \
			  nco:emailAddress(?email) \
			  bound(?work) \
			WHERE \
			{ \
			  { \
			    ?contact rdf:type nco:PersonContact . \
			    { \
			      ?contact nco:hasEmailAddress ?email . \
			    } \
			    UNION \
			    { \
			      ?contact nco:hasAffiliation ?work . \
			      ?work nco:hasEmailAddress ?email . \
			    } \
			  } \
			} \
			ORDER BY ?_contact LIMIT 50 \
			"

	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 50 contacts email information (modified) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(contacts.test_contacts_06, query)
		t.rows.append(hd.TableRow([contacts.test_contacts_06.__name__, value[0], value[1], list, value[2], len(result), l]))

	def test_contacts_07 (self):

		"""
	        Time taken to get 50 contacts online information (original)
		@param description: Time taken to get 50 contacts online information (original)
		@param expected_value: 
		@param querytype: Original
		"""

		query = " \
			SELECT  \
			  ?_contact \
			  ?_OnlineAccount \
			  ?_OnlineAccount_AccountUri \
			  ?_OnlineAccount_ServiceProvider \
			  bound(?_OnlineAccount_Capabilities) \
			  AS ?_OnlineAccount_Capabilities_IsBound \
			  (?_OnlineAccount_Capabilities = nco:im-capability-text-chat) \
			  AS ?_OnlineAccount_Capabilities_IsEqual_TextChat \
			  (?_OnlineAccount_Capabilities = nco:im-capability-media-calls) \
			  AS ?_OnlineAccount_Capabilities_IsEqual_MediaCalls \
			  (?_OnlineAccount_Capabilities = nco:im-capability-audio-calls) \
			  AS ?_OnlineAccount_Capabilities_IsEqual_AudioCalls \
			  (?_OnlineAccount_Capabilities = nco:im-capability-video-calls) \
			  AS ?_OnlineAccount_Capabilities_IsEqual_VideoCalls \
			  (?_OnlineAccount_Capabilities = nco:im-capability-upgrading-calls) \
			  AS ?_OnlineAccount_Capabilities_IsEqual_UpgradingCalls \
			  (?_OnlineAccount_Capabilities = nco:im-capability-file-transfers) \
			  AS ?_OnlineAccount_Capabilities_IsEqual_FileTransfers \
			  (?_OnlineAccount_Capabilities = nco:im-capability-stream-tubes) \
			  AS ?_OnlineAccount_Capabilities_IsEqual_StreamTubes \
			  (?_OnlineAccount_Capabilities = nco:im-capability-dbus-tubes) \
			  AS ?_OnlineAccount_Capabilities_IsEqual_DBusTubes \
			  bound(?_OnlineAccount_Context_Work) \
			  AS ?_OnlineAccount_Context_Work_IsBound \
			WHERE \
			{ \
			  { \
			    ?_contact rdf:type nco:PersonContact . \
			      { \
				?_contact nco:hasIMAddress ?_OnlineAccount . \
				?_OnlineAccount nco:imID ?_OnlineAccount_AccountUri . \
				?_OnlineAccount nco:imCapability ?_OnlineAccount_Capabilities . \
				OPTIONAL { ?_OnlineAccount_ServiceProvider nco:hasIMContact ?_OnlineAccount . } \
			      } \
			      UNION \
			      { \
				?_contact nco:hasAffiliation ?_OnlineAccount_Context_Work . \
				?_OnlineAccount_Context_Work nco:hasIMAddress ?_OnlineAccount .\
				?_OnlineAccount nco:imID ?_OnlineAccount_AccountUri . \
				?_OnlineAccount nco:imCapability ?_OnlineAccount_Capabilities . \
				OPTIONAL { ?_OnlineAccount_ServiceProvider nco:hasIMContact ?_OnlineAccount . } \
			      } \
			  } \
			} \
			ORDER BY ?_contact LIMIT 50 \
			"

	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 50 contacts online information (original) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(contacts.test_contacts_07, query)
		t.rows.append(hd.TableRow([contacts.test_contacts_07.__name__, value[0], value[1], list, value[2], len(result), l]))


	def test_contacts_08 (self):

		"""
	        Time taken to get 50 contacts online information (modified)
		@param description: Time taken to get 50 contacts online information (modified)
		@param expected_value: 
		@param querytype: Modified
		"""

		query = " \
			SELECT  \
			  ?_contact \
			  ?_OnlineAccount \
			  ?_OnlineAccount_AccountUri \
			  ?_OnlineAccount_ServiceProvider \
			  bound(?ork) \
			WHERE \
			{ \
			    ?_contact rdf:type nco:PersonContact . \
			      { \
				?_contact nco:hasIMAddress ?_OnlineAccount . \
				?_OnlineAccount nco:imID ?_OnlineAccount_AccountUri . \
				OPTIONAL { ?_OnlineAccount_ServiceProvider nco:hasIMContact ?_OnlineAccount . } \
			      } \
			      UNION \
			      { \
				?_contact nco:hasAffiliation ?_OnlineAccount_Context_Work . \
				?_OnlineAccount_Context_Work nco:hasIMAddress ?_OnlineAccount . \
				?_OnlineAccount nco:imID ?_OnlineAccount_AccountUri . \
				OPTIONAL { ?_OnlineAccount_ServiceProvider nco:hasIMContact ?_OnlineAccount . } \
			      } \
			} \
			ORDER BY ?_contact LIMIT 50 \
			"

	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 50 contacts online information (modified) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(contacts.test_contacts_08, query)
		t.rows.append(hd.TableRow([contacts.test_contacts_08.__name__, value[0], value[1], list, value[2], len(result), l]))

	def test_contacts_09 (self):

		"""
	        Time taken to get 50 contacts phone number information (original)
		@param description: Time taken to get 50 contacts phone number information (original)
		@param expected_value: 
		@param querytype: Original
		"""

		query = " \
			SELECT \
			  ?_contact ?_PhoneNumber ?_PhoneNumber_PhoneNumber \
			  bound(?_PhoneNumber_SubTypes_BulletinBoardSystem) AS ?_PhoneNumber_SubTypes_BulletinBoardSystem_IsBound \
			  bound(?_PhoneNumber_SubTypes_Car) AS ?_PhoneNumber_SubTypes_Car_IsBound \
			  bound(?_PhoneNumber_SubTypes_Fax) AS ?_PhoneNumber_SubTypes_Fax_IsBound \
			  bound(?_PhoneNumber_SubTypes_MessagingCapable) AS ?_PhoneNumber_SubTypes_MessagingCapable_IsBound \
			  bound(?_PhoneNumber_SubTypes_Mobile) AS ?_PhoneNumber_SubTypes_Mobile_IsBound \
			  bound(?_PhoneNumber_SubTypes_Modem) AS ?_PhoneNumber_SubTypes_Modem_IsBound \
			  bound(?_PhoneNumber_SubTypes_Pager) AS ?_PhoneNumber_SubTypes_Pager_IsBound \
			  bound(?_PhoneNumber_SubTypes_Video) AS ?_PhoneNumber_SubTypes_Video_IsBound \
			  bound(?_PhoneNumber_SubTypes_Voice) AS ?_PhoneNumber_SubTypes_Voice_IsBound \
			  bound(?_PhoneNumber_Context_Work) AS ?_PhoneNumber_Context_Work_IsBound \
			WHERE \
			{ \
			  { \
			    ?_contact rdf:type nco:PersonContact . \
			      { \
				?_contact nco:hasPhoneNumber ?_PhoneNumber . \
				?_PhoneNumber nco:phoneNumber ?_PhoneNumber_PhoneNumber . \
				  OPTIONAL \
				  { \
				    ?_PhoneNumber rdf:type ?_PhoneNumber_SubTypes_BulletinBoardSystem . \
				    FILTER((?_PhoneNumber_SubTypes_BulletinBoardSystem = nco:BbsNumber)) . \
				  } \
				  OPTIONAL \
				  { \
				    ?_PhoneNumber rdf:type ?_PhoneNumber_SubTypes_Car . \
				    FILTER((?_PhoneNumber_SubTypes_Car = nco:CarPhoneNumber)) . \
				  } \
				  OPTIONAL \
				  { \
				    ?_PhoneNumber rdf:type ?_PhoneNumber_SubTypes_Fax . \
				    FILTER((?_PhoneNumber_SubTypes_Fax = nco:FaxNumber)) . \
				  } \
				  OPTIONAL \
				  { \
				     ?_PhoneNumber rdf:type ?_PhoneNumber_SubTypes_MessagingCapable . \
				     FILTER((?_PhoneNumber_SubTypes_MessagingCapable = nco:MessagingNumber)) . \
				  } \
				  OPTIONAL \
				  { \
				    ?_PhoneNumber rdf:type ?_PhoneNumber_SubTypes_Mobile . \
				    FILTER((?_PhoneNumber_SubTypes_Mobile = nco:CellPhoneNumber)) . \
				  } \
				  OPTIONAL \
				  { \
				    ?_PhoneNumber rdf:type ?_PhoneNumber_SubTypes_Modem . \
				    FILTER((?_PhoneNumber_SubTypes_Modem = nco:ModemNumber)) . \
				  } \
				  OPTIONAL \
				  { \
				    ?_PhoneNumber rdf:type ?_PhoneNumber_SubTypes_Pager . \
				    FILTER((?_PhoneNumber_SubTypes_Pager = nco:PagerNumber)) . \
				  } \
				  OPTIONAL \
				  { \
				    ?_PhoneNumber rdf:type ?_PhoneNumber_SubTypes_Video . \
				    FILTER((?_PhoneNumber_SubTypes_Video = nco:VideoTelephoneNumber)) . \
				  } \
				  OPTIONAL \
				  { \
				    ?_PhoneNumber rdf:type ?_PhoneNumber_SubTypes_Voice . \
				    FILTER((?_PhoneNumber_SubTypes_Voice = nco:VoicePhoneNumber)) . \
				  } \
			      } \
			      UNION \
			      { \
				?_contact nco:hasAffiliation ?_PhoneNumber_Context_Work . \
				?_PhoneNumber_Context_Work nco:hasPhoneNumber ?_PhoneNumber . \
				?_PhoneNumber nco:phoneNumber ?_PhoneNumber_PhoneNumber . \
				  OPTIONAL \
				  { \
				    ?_PhoneNumber rdf:type ?_PhoneNumber_SubTypes_BulletinBoardSystem . \
				    FILTER((?_PhoneNumber_SubTypes_BulletinBoardSystem = nco:BbsNumber)) . \
				  } \
				  OPTIONAL \
				  { \
				    ?_PhoneNumber rdf:type ?_PhoneNumber_SubTypes_Car . \
				    FILTER((?_PhoneNumber_SubTypes_Car = nco:CarPhoneNumber)) . \
				  } \
				  OPTIONAL \
				  { \
				    ?_PhoneNumber rdf:type ?_PhoneNumber_SubTypes_Fax . \
				    FILTER((?_PhoneNumber_SubTypes_Fax = nco:FaxNumber)) . \
				  } \
				  OPTIONAL \
				  { \
				    ?_PhoneNumber rdf:type ?_PhoneNumber_SubTypes_MessagingCapable . \
				    FILTER((?_PhoneNumber_SubTypes_MessagingCapable = nco:MessagingNumber)) . \
				  } \
				  OPTIONAL \
				  { \
				    ?_PhoneNumber rdf:type ?_PhoneNumber_SubTypes_Mobile . \
				    FILTER((?_PhoneNumber_SubTypes_Mobile = nco:CellPhoneNumber)) . \
				  } \
				  OPTIONAL \
				  { \
				    ?_PhoneNumber rdf:type ?_PhoneNumber_SubTypes_Modem . \
				    FILTER((?_PhoneNumber_SubTypes_Modem = nco:ModemNumber)) . \
				  } \
				  OPTIONAL \
				  { \
				    ?_PhoneNumber rdf:type ?_PhoneNumber_SubTypes_Pager . \
				    FILTER((?_PhoneNumber_SubTypes_Pager = nco:PagerNumber)) . \
				  } \
				  OPTIONAL \
				  { \
				    ?_PhoneNumber rdf:type ?_PhoneNumber_SubTypes_Video . \
				    FILTER((?_PhoneNumber_SubTypes_Video = nco:VideoTelephoneNumber)) . \
				  } \
				  OPTIONAL \
				  { \
				    ?_PhoneNumber rdf:type ?_PhoneNumber_SubTypes_Voice . \
				    FILTER((?_PhoneNumber_SubTypes_Voice = nco:VoicePhoneNumber)) . \
				  } \
			      } \
			  } \
			} \
			ORDER BY ?_contact LIMIT 50 \
			"

	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 50 contacts phone number information (original) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(contacts.test_contacts_09, query)
		t.rows.append(hd.TableRow([contacts.test_contacts_09.__name__, value[0], value[1], list, value[2], len(result), l]))

	def test_contacts_10 (self):

		"""
	        Time taken to get 50 contacts phone number information (modified)
		@param description: Time taken to get 50 contacts phone number information (modified)
		@param expected_value: 
		@param querytype: Modified
		"""

		query = " \
			SELECT \
			  ?contact \
			  ?phoneNumber \
			  nco:phoneNumber(?phoneNumber) \
			  bound(?work) \
			WHERE \
			{ \
			    ?contact rdf:type nco:PersonContact . \
			    { \
				?contact nco:hasPhoneNumber ?phoneNumber . \
			    } \
			    UNION \
			    { \
				?contact nco:hasAffiliation ?work . \
				?work nco:hasPhoneNumber ?phoneNumber . \
			    } \
			} \
			ORDER BY ?_contact LIMIT 50 \
			"

	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 50 contacts phone number information (modified) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(contacts.test_contacts_10, query)
		t.rows.append(hd.TableRow([contacts.test_contacts_10.__name__, value[0], value[1], list, value[2], len(result), l]))

	def test_contacts_11 (self):

		"""
	        Time taken to get 50 contacts phone number information (modified)
		@param description: Time taken to get 50 contacts phone number information (modified)
		@param expected_value: 
		@param querytype: 
		"""

		query = " \
			"

	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 50 contacts phone number information (modified) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(contacts.test_contacts_10, query)
		t.rows.append(hd.TableRow([contacts.test_contacts_10.__name__, value[0], value[1], list, value[2], len(result), l]))

class location (TestUpdate) :

        def test_location_01 (self):

		"""
	        Time taken to get 50 landmarks (original).
		@param description: Time taken to get 50 landmarks (original)
		@param expected_value: 
		@param querytype: Original
		"""

		query = " \
			SELECT \
			  ?urn \
			  ?cLat ?cLon ?cAlt ?cRad \
			  ?nwLat ?nwLon ?nwAlt \
			  ?seLat ?seLon ?seAlt \
			  ?country ?district ?city ?street ?postalcode \
			  nie:title(?urn) \
			  nie:description(?urn) \
			  mlo:belongsToCategory(?urn) \
			WHERE { \
			  ?urn a mlo:Landmark . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asPostalAddress \
				    [ \
				      a nco:PostalAddress ; \
				      nco:country ?country ; \
				      nco:region ?district ; \
				      nco:locality ?city ; \
				      nco:streetAddress ?street ; \
				      nco:postalcode ?postalcode \
				    ] \
				] \
			    } . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asBoundingBox \
				    [ \
				      a mlo:GeoBoundingBox ; \
				      mlo:bbNorthWest \
					[ \
					  a mlo:GeoPoint ; \
					  mlo:latitude ?nwLat ; \
					  mlo:longitude ?nwLon ; \
					  mlo:altitude ?nwAlt \
					] ; \
				      mlo:bbSouthEast \
					[ \
					  a mlo:GeoPoint ; \
					  mlo:latitude ?seLat ; \
					  mlo:longitude ?seLon ; \
					  mlo:altitude ?seAlt \
					] \
				    ] \
				] \
			    } . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asGeoPoint \
				    [ \
				      a mlo:GeoPoint ; \
				      mlo:latitude ?cLat ; \
				      mlo:longitude ?cLon \
				    ] \
				] \
			    } . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asGeoPoint \
				    [ \
				      a mlo:GeoPoint ; \
				      mlo:altitude ?cAlt \
				    ] \
				] \
			    } . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asGeoPoint \
				    [ \
				      a mlo:GeoPoint ; \
				      mlo:radius ?cRad \
				    ] \
				] \
			    } \
			} ORDER BY ASC(?name) LIMIT 50 \
			"

	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 50 landmarks (original) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(location.test_location_01, query)
		t.rows.append(hd.TableRow([location.test_location_01.__name__, value[0], value[1], list, value[2], len(result), l]))

        def test_location_02 (self):

		"""
	        Time taken to get 50 landmarks within coords (original)
		@param description: Time taken to get 50 landmarks within coords (original)
		@param expected_value: 
		@param querytype: Original
		"""

		query = " \
			SELECT \
			  ?urn \
			  ?cLat ?cLon ?cAlt ?cRad \
			  ?nwLat ?nwLon ?nwAlt \
			  ?seLat ?seLon ?seAlt \
			  ?country ?district ?city ?street ?postalcode \
			  nie:title(?urn) \
			  nie:description(?urn) \
			  mlo:belongsToCategory(?urn) \
			WHERE { \
			  ?urn a mlo:Landmark . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asPostalAddress \
				    [ \
				      a nco:PostalAddress ; \
				      nco:country ?country ; \
				      nco:region ?district ; \
				      nco:locality ?city ; \
				      nco:streetAddress ?street ; \
				      nco:postalcode ?postalcode \
				    ] \
				] \
			    } . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asBoundingBox \
				    [ \
				      a mlo:GeoBoundingBox ; \
				      mlo:bbNorthWest \
					[ \
					  a mlo:GeoPoint ; \
					  mlo:latitude ?nwLat ; \
					  mlo:longitude ?nwLon ; \
					  mlo:altitude ?nwAlt \
					] ; \
				      mlo:bbSouthEast \
					[ \
					  a mlo:GeoPoint ; \
					  mlo:latitude ?seLat ; \
					  mlo:longitude ?seLon ; \
					  mlo:altitude ?seAlt \
					] \
				    ] \
				] \
			    } . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asGeoPoint \
				    [ \
				      a mlo:GeoPoint ; \
				      mlo:latitude ?cLat ; \
				      mlo:longitude ?cLon \
				    ] \
				] \
			    } . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asGeoPoint \
				    [ \
				      a mlo:GeoPoint ; \
				      mlo:altitude ?cAlt \
				    ] \
				] \
			    } . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asGeoPoint \
				    [ \
				      a mlo:GeoPoint ; \
				      mlo:radius ?cRad \
				    ] \
				] \
			    } \
			  FILTER(?cLat >= 39.16 && ?cLat <= 40.17 && ?cLon >= 63.94 && ?cLon <= 64.96) \
			} ORDER BY ASC(?name) LIMIT \
			"

	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 50 landmarks within coords (original) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(location.test_location_02, query)
		t.rows.append(hd.TableRow([location.test_location_02.__name__, value[0], value[1], list, value[2], len(result), l]))


        def test_location_03 (self):

		"""
	        Time taken to get max 50 landmarks within certain range with bounding box (original).
		@param description: Time taken to get max 50 landmarks within certain range with bounding box (original)
		@param expected_value: 
		@param querytype: Original
		"""

		query = " \
			SELECT \
			  ?urn \
			  ?cLat ?cLon ?cAlt ?cRad \
			  ?nwLat ?nwLon ?nwAlt \
			  ?seLat ?seLon ?seAlt \
			  ?country ?district ?city ?street ?postalcode \
			  nie:title(?urn) \
			  nie:description(?urn) \
			  mlo:belongsToCategory(?urn) \
			  tracker:haversine-distance(xsd:double(?cLat),xsd:double(39.50),xsd:double(?cLon),xsd:double(64.50)) as ?distance \
			WHERE { \
			  ?urn a mlo:Landmark . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asPostalAddress \
				    [ \
				      a nco:PostalAddress ; \
				      nco:country ?country ; \
				      nco:region ?district ; \
				      nco:locality ?city ; \
				      nco:streetAddress ?street ; \
				      nco:postalcode ?postalcode \
				    ] \
				] \
			    } . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asBoundingBox \
				    [ \
				      a mlo:GeoBoundingBox ; \
				      mlo:bbNorthWest \
					[ \
					  a mlo:GeoPoint ; \
					  mlo:latitude ?nwLat ; \
					  mlo:longitude ?nwLon ; \
					  mlo:altitude ?nwAlt \
					] ; \
				      mlo:bbSouthEast \
					[ \
					  a mlo:GeoPoint ; \
					  mlo:latitude ?seLat ; \
					  mlo:longitude ?seLon ; \
					  mlo:altitude ?seAlt \
					] \
				    ] \
				] \
			    } . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asGeoPoint \
				    [ \
				      a mlo:GeoPoint ; \
				      mlo:latitude ?cLat ; \
				      mlo:longitude ?cLon \
				    ] \
				] \
			    } . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asGeoPoint \
				    [ \
				      a mlo:GeoPoint ; \
				      mlo:altitude ?cAlt \
				    ] \
				] \
			    } . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asGeoPoint \
				    [ \
				      a mlo:GeoPoint ; \
				      mlo:radius ?cRad \
				    ] \
				] \
			    } \
			  FILTER(?cLat >= 39.16 && ?cLat <= 40.17 && \
				 ?cLon >= 63.94 && ?cLon <= 64.96 && \
				 tracker:haversine-distance(xsd:double(?cLat),xsd:double(39.50),xsd:double(?cLon),xsd:double(64.50)) <= 25000) \
			} ORDER BY ASC(?distance) LIMIT 50 \
			"
	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get max 50 landmarks within certain range with bounding box (original) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(location.test_location_03, query)
		t.rows.append(hd.TableRow([location.test_location_03.__name__, value[0], value[1], list, value[2], len(result), l]))


        def test_location_04 (self):

		"""
	        Time taken to get max 50 landmarks within certain range without bounding box (original).	
		@param description: Time taken to get max 50 landmarks within certain range without bounding box (original)
		@param expected_value: 
		@param querytype: Original
		"""

		query = " \
			SELECT \
			  ?urn \
			  ?cLat ?cLon ?cAlt ?cRad \
			  ?nwLat ?nwLon ?nwAlt \
			  ?seLat ?seLon ?seAlt \
			  ?country ?district ?city ?street ?postalcode \
			  nie:title(?urn) \
			  nie:description(?urn) \
			  mlo:belongsToCategory(?urn) \
			  tracker:haversine-distance(xsd:double(?cLat),xsd:double(39.50),xsd:double(?cLon),xsd:double(64.50)) as ?distance \
			WHERE { \
			  ?urn a mlo:Landmark . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asPostalAddress \
				    [ \
				      a nco:PostalAddress ; \
				      nco:country ?country ; \
				      nco:region ?district ; \
				      nco:locality ?city ; \
				      nco:streetAddress ?street ; \
				      nco:postalcode ?postalcode \
				    ] \
				] \
			    } . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asBoundingBox \
				    [ \
				      a mlo:GeoBoundingBox ; \
				      mlo:bbNorthWest \
					[ \
					  a mlo:GeoPoint ; \
					  mlo:latitude ?nwLat ; \
					  mlo:longitude ?nwLon ; \
					  mlo:altitude ?nwAlt \
					] ; \
				      mlo:bbSouthEast \
					[ \
					  a mlo:GeoPoint ; \
					  mlo:latitude ?seLat ; \
					  mlo:longitude ?seLon ; \
					  mlo:altitude ?seAlt \
					] \
				    ] \
				] \
			    } . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asGeoPoint \
				    [ \
				      a mlo:GeoPoint ; \
				      mlo:latitude ?cLat ; \
				      mlo:longitude ?cLon \
				    ] \
				] \
			    } . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asGeoPoint \
				    [ \
				      a mlo:GeoPoint ; \
				      mlo:altitude ?cAlt \
				    ] \
				] \
			    } . \
			  OPTIONAL \
			    { \
			      ?urn mlo:location \
				[ \
				  a mlo:GeoLocation ; \
				  mlo:asGeoPoint \
				    [ \
				      a mlo:GeoPoint ; \
				      mlo:radius ?cRad \
				    ] \
				] \
			    } \
			  FILTER(tracker:haversine-distance(xsd:double(?cLat),xsd:double(39.50),xsd:double(?cLon),xsd:double(64.50)) <= 25000) \
			} ORDER BY ASC(?distance) LIMIT 50 \
			"
	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get max 50 landmarks within certain range without bounding box (original) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(location.test_location_04, query)
		t.rows.append(hd.TableRow([location.test_location_04.__name__, value[0], value[1], list, value[2], len(result), l]))

        def test_location_05 (self):

		"""
	        Time taken to get 50 landmarks (simplified).	
		@param description: Time taken to get 50 landmarks (simplified)
		@param expected_value: 6.42
		@param querytype: 
		"""

		query = " \
			SELECT \
			  ?urn \
			  mlo:latitude(?point) mlo:longitude(?point) mlo:altitude(?point) mlo:radius(?point) \
			  nie:title(?urn) \
			  nie:description(?urn) \
			  mlo:belongsToCategory(?urn) \
			WHERE { \
			  ?urn a mlo:Landmark . \
			  ?urn mlo:location ?location . \
			  ?location mlo:asGeoPoint ?point . \
			} ORDER BY ASC(?name) LIMIT 50 \
			"
	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get 50 landmarks (simplified) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(location.test_location_05, query)
		t.rows.append(hd.TableRow([location.test_location_05.__name__, value[0], value[1], list, value[2], len(result), l]))


        def test_location_06 (self):

		"""
	        Time taken to get max 50 landmarks within coords (simplified).	
		@param description: Time taken to get max 50 landmarks within coords (simplified) 
		@param expected_value: 0.43
		@param querytype: 
		"""

		query = " \
			SELECT \
			  ?urn \
			  ?cLat ?cLon mlo:altitude(?point) mlo:radius(?point) \
			  nie:title(?urn) \
			  nie:description(?urn) \
			  mlo:belongsToCategory(?urn) \
			WHERE { \
			  ?urn a mlo:Landmark . \
			  ?urn mlo:location ?location . \
			  ?location mlo:asGeoPoint ?point . \
			  ?point mlo:latitude ?cLat . \
			  ?point mlo:longitude ?cLon . \
			  FILTER(?cLat >= 39.16 && ?cLat <= 40.17 && ?cLon >= 63.42 && ?cLon <= 64.96) \
			} ORDER BY ASC(?name) LIMIT 50 \
			"
	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get max 50 landmarks within coords (simplified) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(location.test_location_06, query)
		t.rows.append(hd.TableRow([location.test_location_06.__name__, value[0], value[1], list, value[2], len(result), l]))

        def test_location_07 (self):

		"""
	        Time taken to get max 50 landmarks within range with bounding box (simplified).	
		@param description: Time taken to get max 50 landmarks within range with bounding box (simplified)
		@param expected_value: 0.44
		@param querytype: 
		"""

		query = " \
			SELECT \
			  ?urn \
			  ?cLat ?cLon mlo:altitude(?point) mlo:radius(?point) \
			  nie:title(?urn) \
			  nie:description(?urn) \
			  mlo:belongsToCategory(?urn) \
			  tracker:haversine-distance(xsd:double(?cLat),xsd:double(39.50),xsd:double(?cLon),xsd:double(64.50)) as ?distance \
			WHERE { \
			  ?urn a mlo:Landmark . \
			  ?urn mlo:location ?location . \
			  ?location mlo:asGeoPoint ?point . \
			  ?point mlo:latitude ?cLat . \
			  ?point mlo:longitude ?cLon . \
			  FILTER(?cLat >= 39.16 && ?cLat <= 40.17 && \
				 ?cLon >= 63.94 && ?cLon <= 64.96 && \
				 tracker:haversine-distance(xsd:double(?cLat),xsd:double(39.50),xsd:double(?cLon),xsd:double(64.50)) <= 25000) \
			} ORDER BY ASC(?distance) LIMIT 50 \
			"
	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get max 50 landmarks within range with bounding box (simplified) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(location.test_location_07, query)
		t.rows.append(hd.TableRow([location.test_location_07.__name__, value[0], value[1], list, value[2], len(result), l]))

        def test_location_08 (self):

		"""
	        Time taken to get max 50 landmarks within range without bounding box (simplified).	
		@param description: Time taken to get max 50 landmarks within range without bounding box (simplified)
		@param expected_value: 0.63
		@param querytype: 
		"""

		query = " \
			SELECT \
			  ?urn \
			  ?cLat ?cLon mlo:altitude(?point) mlo:radius(?point) \
			  nie:title(?urn) \
			  nie:description(?urn) \
			  mlo:belongsToCategory(?urn) \
			  tracker:haversine-distance(xsd:double(?cLat),xsd:double(39.50),xsd:double(?cLon),xsd:double(64.50)) as ?distance \
			WHERE { \
			  ?urn a mlo:Landmark . \
			  ?urn mlo:location ?location . \
			  ?location mlo:asGeoPoint ?point . \
			  ?point mlo:latitude ?cLat . \
			  ?point mlo:longitude ?cLon . \
			  FILTER(tracker:haversine-distance(xsd:double(?cLat),xsd:double(39.50),xsd:double(?cLon),xsd:double(64.50)) <= 25000) \
			} ORDER BY ASC(?distance) LIMIT 50 \
			"
	        list=[]
		for i in range ( 0, iterations ):
			start=time.time()

			result=self.resources.SparqlQuery(query)

			elapse =time.time()-start
			elapse = "%.3f" %elapse 
			print "Time taken to get max 50 landmarks within range without bounding box (simplified) %s " %elapse
			print "no. of items retrieved: %d" %len(result)
		        list.append(elapse)

		value, l=self.parsing(location.test_location_08, query)
		t.rows.append(hd.TableRow([location.test_location_08.__name__, value[0], value[1], list, value[2], len(result), l]))


def header(title):
	global t
	t=hd.Table(width='100%', col_width=('12%', '20%', '5%', '30%', '10%', '10%', '20%'))
	doc.nodes.append(hd.Heading(size='2', heading=title))
	t.rows.append(hd.TableRow(['Test', 'Description', 'ExpectedValue (sec)', 'ActualValue (sec)', 'Query type', 'No of items retrived', 'Query'], header=True))

if __name__ == "__main__":

	if not os.path.isdir("report"):	
		os.mkdir("report")
	global iterations
	iterations=10
        doc=hd.HtmlDoc()
        doc.nodes.append(hd.Heading(size='1', heading="Tracker Performance Test"))
	
        dict={}
        base={"email" : email, "calls": calls, "instant_messages": instant_messages, "rtcom":rtcom, "audio":audio, "gallery":gallery, "ftsmatch":ftsmatch, "content_manager":content_manager, "contacts":contacts, "location":location }
        if len(sys.argv)==1:
	        dict=base
        else:
                for arg in range(1,len(sys.argv)):
                        print sys.argv[arg]
                        dict[sys.argv[arg]]=base[sys.argv[arg]]
	
        for k, v in dict.items():
            header(k)
            unittest.TextTestRunner().run(unittest.TestLoader().loadTestsFromTestCase(v))
            doc.nodes.append(t)

        report=open("./perf_report.html", "a")
        doc.toHtml(report)
        report.close()

	#unittest.main()

