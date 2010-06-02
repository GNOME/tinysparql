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

import sys,os,dbus
import unittest
import time
import random
import string
import datetime

TRACKER = 'org.freedesktop.Tracker1'
TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources'
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources"


class TestUpdate (unittest.TestCase):
	
	def setUp(self):
		bus = dbus.SessionBus()
	        tracker = bus.get_object(TRACKER, TRACKER_OBJ)
		self.resources = dbus.Interface (tracker,
		                                 dbus_interface=RESOURCES_IFACE)


	def sparql_update(self,query):
		return self.resources.SparqlUpdate(query)

	def query(self,query):
		return self.resources.SparqlQuery(query)
	

""" Insertion test cases """
class s_insert(TestUpdate):
	

	def test_insert_01(self):

		"""
                1. Insert a InformationElement with title.
                2. TEST: Query the title of that information element
                3. Remove the InformationElement to keep everything as it was before
                """

                uri = "tracker://test_insert_01/" + str(random.randint (0, 100))
                insert = """
                INSERT { <%s> a nie:InformationElement;
                        nie:title \"test_insert_01\". }
                """ % (uri)
                self.sparql_update (insert)

		""" verify the inserted item """
                query = """
                SELECT ?t WHERE {
                <%s> a nie:InformationElement ;
                nie:title ?t .
                }
                """ % (uri)
                results = self.query (query)

                self.assertEquals (str(results[0][0]), "test_insert_01")

		""" delete the inserted item """
                delete = """
                DELETE { <%s> a nie:InformationElement. }
                """ % (uri)
                self.sparql_update (delete)

		
	def test_insert_02(self):
                ''' SparqlUpdate: Insert triples and check using SparqlQuery'''

                self.sparql_update('INSERT {<urn:uuid:7646007> a nco:Contact; \
                nco:fullname "Artist_1_update". \
                <file:///media/PIKKUTIKKU/5000_songs_with_metadata_and_album_arts/Artist_1/1_Album/10_song3.mp3> a nmm:MusicPiece,nfo:FileDataObject,nmm:MusicAlbum;\
                nfo:fileName "10_song.mp3"; \
                nfo:fileLastModified "2008-10-23T13:47:02" ; \
                nfo:fileCreated "2008-12-16T12:41:20"; \
                nfo:fileSize 17630; \
                nmm:length 219252; \
                nmm:albumTitle "anything".}')

                self.verify_test_insert_02()


        def verify_test_insert_02(self):

                result = self.query('SELECT ?artist ?date  WHERE{ \
                ?contact nco:fullname ?artist . \
		?time nfo:fileCreated ?date . \
                FILTER (?artist = "Artist_1_update") \
                }')
		print result

                for i in range(len(result)):
                        if result[i][0] == 'Artist_1_update':
                                if  result[i][1] == 'anything' and result[i][2] == '219252':
                                        self.assert_(True,'Pass')
					return
                                else:
					if i < range(len(result) - 1):
						continue
					else:
                                        	self.fail('Fail %s' %result)


        def test_insert_03(self):
                ''' SparqlUpdate: Insert triples and check using SparqlQuery'''

		self.sparql_update('INSERT {<urn:uuid:7646004> a nco:Contact; \
                nco:fullname "Artist_4_update". \
                <file:///media/PIKKUTIKKU/5000_songs_with_metadata_and_album_arts/Artist_4/4_Album/4_song_1.mp3> a nmm:MusicPiece,nfo:FileDataObject;\
                nfo:fileName "4_song_1.mp3"; \
                nfo:fileCreated "2008-12-16T12:41:20"; \
                nfo:fileLastModified "2008-12-23T13:47:02" ; \
                nfo:fileSize 17630; \
                nmm:musicAlbum "4_Album_update"; \
                nmm:trackNumber "11"; \
                nmm:length 219252; \
                nmm:performer <urn:uuid:7646004>.}')

                self.verify_test_insert_03()


        def verify_test_insert_03(self):

                result = self.query('SELECT ?artist ?album ?len ?trkNo ?fname ?fSz  WHERE{ \
                ?contact nco:fullname ?artist . \
                ?song nmm:musicAlbum ?album ; \
                nmm:length ?len  ;\
                nmm:trackNumber ?trkNo.\
                ?songfile nfo:fileName ?fname ;\
                nfo:fileSize ?fSz .\
                FILTER (?fname = "4_song_1.mp3") \
                }')
                print len(result)

                for i in range(len(result)):
                        if result[i][0] == 'Artist_4_update':
                                if  result[i][1] == '4_Album_update' and result[i][2] == '219252' and result[i][3] == '11' and result[i][4] == '4_song_1.mp3' and result[i][5] == '17630':
                                        self.assert_(True,'Pass')
					return
                                else:
					if i < range(len(result) - 1):
						continue
					else:
                                        	self.fail('Fail %s' %result)

	def test_insert_04(self):
                """Insert, delete same single valued properties multiple times."""
		
		""" Delete single valued properties of music file.""" 	
		self.sparql_update('DELETE { <file:///media/PIKKUTIKKU/album_4/4_song4.mp3> nie:usageCounter ?v } WHERE { \
		<file:///media/PIKKUTIKKU/album_4/4_song4.mp3> nie:usageCounter ?v . }')
		self.sparql_update('DELETE { <file:///media/PIKKUTIKKU/album_4/4_song4.mp3> nie:contentAccessed ?v } WHERE { \
		<file:///media/PIKKUTIKKU/album_4/4_song4.mp3> nie:contentAccessed ?v . }')
		

		""" Insert the same single valued properties of music file.""" 	
                self.sparql_update('INSERT { \
                <file:///media/PIKKUTIKKU/album_4/4_song4.mp3> a nmm:MusicPiece,nfo:FileDataObject;\
		nie:usageCounter "1"; \
		nie:contentAccessed "2000-01-01T00:40:47Z" . }')

		""" Delete again the single valued properties of music file.""" 	
		self.sparql_update('DELETE { <file:///media/PIKKUTIKKU/album_4/4_song4.mp3> nie:usageCounter ?v } WHERE { \
		<file:///media/PIKKUTIKKU/album_4/4_song4.mp3> nie:usageCounter ?v . }')
		self.sparql_update('DELETE { <file:///media/PIKKUTIKKU/album_4/4_song4.mp3> nie:contentAccessed ?v } WHERE { \
		<file:///media/PIKKUTIKKU/album_4/4_song4.mp3> nie:contentAccessed ?v . }')
		

		""" Insert the same single valued properties of music file with different values.""" 	
                self.sparql_update('INSERT { \
                <file:///media/PIKKUTIKKU/album_4/4_song4.mp3> a nmm:MusicPiece,nfo:FileDataObject;\
		nie:usageCounter "2"; \
		nie:contentAccessed "2000-01-01T00:40:48Z" . }')

		""" Query for the property values and verify whether the last change is applied."""
                result = self.query('SELECT ?song ?time  WHERE{ \
		?song nie:usageCounter "2"; \
		nie:contentAccessed ?time . \
                }')
                print len(result)

                for i in range(len(result)):
			if result[i][0] == 'file:///media/PIKKUTIKKU/album_4/4_song4.mp3':
				if  result[i][1] == '2000-01-01T00:40:48Z':
                                        self.assert_(True,'Pass')
                                else:
                                        self.fail('Fail %s' %result)


	"""Date-Time storage testing """

	def test_insert_date_01(self):

		"""
                1. Insert a InformationElement with date having local timezone info.
                2. TEST: Query and verify the various componentes of date
                """

                uri = "tracker:test_date_01"
                insert = """
                INSERT { <%s> a nie:InformationElement;
			nie:informationElementDate "2004-05-06T13:14:15+0400". }
                """ % (uri)
                self.sparql_update (insert)

		""" verify the inserted item """

		query = 'SELECT ?s fn:year-from-dateTime (?v) \
		fn:month-from-dateTime (?v) \
		fn:day-from-dateTime (?v) \
		fn:hours-from-dateTime (?v) \
		fn:minutes-from-dateTime (?v) \
		fn:seconds-from-dateTime (?v) \
		fn:timezone-from-dateTime (?v) \
		WHERE { ?s a nie:InformationElement; \
		nie:informationElementDate ?v . \
		}'
                result = self.query (query)
		print result

                for i in range(len(result)):
			if result[i][0] == 'tracker:test_date_01':
				if  result[i][1] == '2004' and result[i][2] == '05' and result[i][3] == '06' and result[i][4] == '13' and result[i][5] == '14' and result[i][6] == '15' and result[i][7] == '14400' :
                                        self.assert_(True,'Pass')
                                else:
                                        self.fail('Fail %s' %result)

	def test_insert_date_02(self):

		"""
                1. Insert a InformationElement with invalid year in date.
                2. TEST: Query and verify the various componentes of date
                """

                uri = "tracker:test_date_02"
                insert = """
                INSERT { <%s> a nie:InformationElement;
			nie:informationElementDate "204-05-06T13:14:15+0400". }
                """ % (uri)
		try : 
			self.resources.SparqlUpdate(insert)

		except :
			print "error in query execution"
			self.assert_(True,'error in query execution')
			
		""" verify whether the item is inserted"""

		query = 'SELECT ?s fn:year-from-dateTime (?v) \
		fn:month-from-dateTime (?v) \
		fn:day-from-dateTime (?v) \
		fn:hours-from-dateTime (?v) \
		fn:minutes-from-dateTime (?v) \
		fn:seconds-from-dateTime (?v) \
		fn:timezone-from-dateTime (?v) \
		WHERE { ?s a nie:InformationElement; \
		nie:informationElementDate ?v . \
		}'
                result = self.query (query)
		print result

                for i in range(len(result)):
			if result[i][0] == 'tracker:test_date_02':
				if  result[i][1] != '204':
                                        self.assert_(True,'Pass')
                                else:
                                        self.fail('Fail %s' %result)


	def test_insert_date_03(self):

		"""
                1. Insert a InformationElement with date ending with "Z" in TZD.
                2. TEST: Query and verify the various componentes of date
                """

                uri = "tracker:test_date_03"
                insert = """
                INSERT { <%s> a nie:InformationElement;
			nie:informationElementDate "2004-05-06T13:14:15Z". }
                """ % (uri)
                self.sparql_update (insert)

		""" verify the inserted item """

		query = 'SELECT ?s fn:year-from-dateTime (?v) \
		fn:month-from-dateTime (?v) \
		fn:day-from-dateTime (?v) \
		fn:hours-from-dateTime (?v) \
		fn:minutes-from-dateTime (?v) \
		fn:seconds-from-dateTime (?v) \
		fn:timezone-from-dateTime (?v) \
		WHERE { ?s a nie:InformationElement; \
		nie:informationElementDate ?v . \
		}'
                result = self.query (query)
		print result

                for i in range(len(result)):
			if result[i][0] == 'tracker:test_date_03':
				if  result[i][1] == '2004' and result[i][2] == '05' and result[i][3] == '06' and result[i][4] == '13' and result[i][5] == '14' and result[i][6] == '15' and result[i][7] == '0' :
                                        self.assert_(True,'Pass')
                                else:
                                        self.fail('Fail %s' %result)


	def test_insert_date_04(self):

		"""
                1. Insert a InformationElement with date without TZD.
                2. TEST: Query and verify the various componentes of date
                """

                uri = "tracker:test_date_04"
                insert = """
                INSERT { <%s> a nie:InformationElement;
			nie:informationElementDate "2004-05-06T13:14:15". }
                """ % (uri)
                self.sparql_update (insert)

		""" verify the inserted item """

		query = 'SELECT ?s fn:year-from-dateTime (?v) \
		fn:month-from-dateTime (?v) \
		fn:day-from-dateTime (?v) \
		fn:hours-from-dateTime (?v) \
		fn:minutes-from-dateTime (?v) \
		fn:seconds-from-dateTime (?v) \
		fn:timezone-from-dateTime (?v) \
		WHERE { ?s a nie:InformationElement; \
		nie:informationElementDate ?v . \
		}'
                result = self.query (query)
		print result

                for i in range(len(result)):
			if result[i][0] == 'tracker:test_date_04':
				if  result[i][1] == '2004' and result[i][2] == '05' and result[i][3] == '06' and result[i][4] == '13' and result[i][5] == '14' and result[i][6] == '15':
                                        self.assert_(True,'Pass')
                                else:
                                        self.fail('Fail %s' %result)


	def test_insert_date_05(self):

		"""
                1. Insert a InformationElement with date without time.
                2. TEST: Query and verify the various componentes of date
                """

                uri = "tracker:test_date_05"
                insert = """
                INSERT { <%s> a nie:InformationElement;
			nie:informationElementDate "2004-05-06". }
                """ % (uri)
		try : 
			self.resources.SparqlUpdate(insert)

		except :
			print "error in query execution"
			self.assert_(True,'error in query execution')

		else:
			self.fail('Query successfully executed')		
			


	def test_insert_date_06(self):

		"""
                1. Insert a InformationElement with date without time but only the "T" separator.
                """

                uri = "tracker:test_date_06"
                insert = """
                INSERT { <%s> a nie:InformationElement;
			nie:informationElementDate "2004-05-06T". }
                """ % (uri)

		try : 
			self.resources.SparqlUpdate(insert)

		except :
			print "error in query execution"
			self.assert_(True,'error in query execution')

		else:
			self.fail('Query successfully executed')		
			

	def test_insert_date_07(self):

		"""
                1. Insert a InformationElement with date having local timezone info
		   with some minutes in it.
                2. TEST: Query and verify the various componentes of date
                """

                uri = "tracker:test_date_07"
                insert = """
                INSERT { <%s> a nie:InformationElement;
			nie:informationElementDate "2004-05-06T13:14:15+0230". }
                """ % (uri)
                self.sparql_update (insert)

		""" verify the inserted item """

		query = 'SELECT ?s fn:year-from-dateTime (?v) \
		fn:month-from-dateTime (?v) \
		fn:day-from-dateTime (?v) \
		fn:hours-from-dateTime (?v) \
		fn:minutes-from-dateTime (?v) \
		fn:seconds-from-dateTime (?v) \
		fn:timezone-from-dateTime (?v) \
		WHERE { ?s a nie:InformationElement; \
		nie:informationElementDate ?v . \
		}'
                result = self.query (query)

                for i in range(len(result)):
			if result[i][0] == 'tracker:test_date_07':
				if  result[i][1] == '2004' and result[i][2] == '05' and result[i][3] == '06' and result[i][4] == '13' and result[i][5] == '14' and result[i][6] == '15' and result[i][7] == '9000':
                                        self.assert_(True,'Pass')
                                else:
					print result[i]
                                        self.fail('Fail %s' %result)

	def test_insert_date_08(self):

		"""
                1. Insert a InformationElement with date having 
		local timezone info in negative.
                2. TEST: Query and verify the various componentes of date
                """

                uri = "tracker:test_date_08"
                insert = """
                INSERT { <%s> a nie:InformationElement;
			nie:informationElementDate "2004-05-06T13:14:15-0230". }
                """ % (uri)
                self.sparql_update (insert)

		""" verify the inserted item """

		query = 'SELECT ?s fn:year-from-dateTime (?v) \
		fn:month-from-dateTime (?v) \
		fn:day-from-dateTime (?v) \
		fn:hours-from-dateTime (?v) \
		fn:minutes-from-dateTime (?v) \
		fn:seconds-from-dateTime (?v) \
		fn:timezone-from-dateTime (?v) \
		WHERE { ?s a nie:InformationElement; \
		nie:informationElementDate ?v . \
		}'
                result = self.query (query)

                for i in range(len(result)):
			if result[i][0] == 'tracker:test_date_08':
				if  result[i][1] == '2004' and result[i][2] == '05' and result[i][3] == '06' and result[i][4] == '13' and result[i][5] == '14' and result[i][6] == '15' and result[i][7] == '-9000':
                                        self.assert_(True,'Pass')
                                else:
					print result[i]
                                        self.fail('Fail %s' %result)

	def test_insert_date_09(self):

		"""
                1. Insert a InformationElement with date having some letters instead of numbers
                2. TEST: Query and verify the various componentes of date
                """

                uri = "tracker:test_date_09"
                insert = """
                INSERT { <%s> a nie:InformationElement;
			nie:informationElementDate "2004-05-06T1g:14:15-0200". }
                """ % (uri)

		try : 
			self.resources.SparqlUpdate(insert)

		except :
			print "error in query execution"
			self.assert_(True,'error in query execution')

		else:
			self.fail('Query successfully executed')		
			

""" Deletion test cases """
class s_delete(TestUpdate):

        def test_delete_01(self):

                ''' Insert triples and Delete a triple. Verify the deletion with a query'''

		"""first insert """
                self.sparql_update('INSERT {<urn:uuid:7646001> a nco:Contact; \
                nco:fullname "Artist_1_delete". \
                <file:///media/PIKKUTIKKU/5000_songs_with_metadata_and_album_arts/Artist_1/1_Album/11_song_del.mp3> a nmm:MusicPiece,nfo:FileDataObject;\
                nfo:fileName "11_song_del.mp3"; \
                nfo:genre "Classic delete"; \
                nmm:musicAlbum "1_Album_delete"; \
                nmm:performer <urn:uuid:7646001>.}')

		"""verify the insertion """
                self.verify_test_insert_delete_01()

		"""now  delete """
                self.sparql_update('DELETE { \
                <file:///media/PIKKUTIKKU/5000_songs_with_metadata_and_album_arts/Artist_1/1_Album/11_song_del.mp3> nfo:genre "Classic delete".}')
                print " After deleting a triple"
		"""verify the deletion """
        	self.verify_test_delete_01()


        def verify_test_insert_delete_01(self):
                result = self.query('SELECT ?fname ?genre WHERE  { \
                ?songfile nfo:fileName ?fname ;\
                nfo:genre ?genre .\
                FILTER (?genre = "Classic delete") \
                }')
                if result != []:
                        for i in range(len(result)):
                                if result[i][0] == '11_song_del.mp3':
                                        if result[i][1] == "Classic delete":
                                                self.assert_(True,'Pass')
					else:
                                                self.fail('File not inserted, so failing the \'Delete genre \' testcase')
                else:
                        self.fail('File not inserted, so failing the \'Delete genre \' testcase')

        def verify_test_delete_01(self):

                result = self.query('SELECT ?fn  WHERE { \
                ?f a nmm:MusicPiece . \
                ?f nfo:fileName ?fn ;\
                nfo:genre ?genre .\
                FILTER (?genre = "Classic delete") \
                 }' )

                if result != []:
                        for i in range(len(result)):
                                if result[i][0] == '11_song_del.mp3':
                                        self.fail('Fail %s' %result)
                                else:
                                        self.assert_(True,'Pass')
                else:
                        self.assert_(True,'Pass')


	def test_delete_02 (self):

		"""Delete a MusicAlbum and count the album """
		"""
		1. add a music album.
		2. count the number of albums
		3. delete an album
		2. count the number of albums
		"""

		"""Add a music album """
                self.sparql_update('INSERT {<06_Album_delete> a nmm:MusicAlbum;\
                nmm:albumTitle "06_Album_delete".}')

		"""get the count of music albums"""
                result = self.query('SELECT ?album  WHERE  { \
                ?album a nmm:MusicAlbum. \
                } ')
                count_before_del = len(result)
                print len(result)
                print result


		"""Delete the added  music album """
                self.sparql_update('DELETE { \
                <06_Album_delete> a nmm:MusicAlbum.}')

		"""get the count of music albums"""
                result = self.query('SELECT ?album  WHERE  { \
                ?album a nmm:MusicAlbum. \
                } ')

                count_after_del = len(result)
                print len(result)

                self.assertEquals (count_before_del - 1, count_after_del)


""" Batch Update test cases """
class s_batch_update(TestUpdate):

	def test_batch_insert_01(self):
		"""batch insertion of 100 contacts:
		1. delete those existing contacts which we want to insert again.
		2. insert 100 contacts.
		3. delete the inserted contacts.
		"""
		

		"""delete those existing contacts which we want to insert again.
		the uid creation is same here as it's created during insertion."""
		
     		for j in range(100) :
			uid = j*1000+1234
			delete = 'DELETE {?contact a rdfs:Resource} WHERE {?contact nco:contactUID  <%s> }' %uid
			self.sparql_update (delete)

		""" querry no. of existing contacts. """
		result = self.query ('SELECT ?c ?Fname ?Gname ?number WHERE { \
					?c a  nco:PersonContact ; \
					nco:nameGiven ?Gname ; \
					nco:nameFamily ?Fname; \
					nco:hasPhoneNumber ?number. \
					} LIMIT 1000')

		count_before_insert = len(result)
		print "contact count before insert %d" %count_before_insert

		"""insert 100 contacts."""
		INSERT_SPARQL = "; ".join([
				"<%s> a nco:PersonContact",
				"nco:nameGiven '%s'",
				"nco:nameFamily '%s'",
				"nco:contactUID %s",
				"nco:hasPhoneNumber <tel:%s>."
				])
		a="\n"
     		"""Preparing a list of Contacts """
     		for j in range(100) :
       			contact=  str(random.randint(0, sys.maxint))
       			names1=['christopher','paul','timothy','stephen','michael','andrew','harold','douglas','timothy','walter','kevin','joshua','robert','matthew','broderick','lacy','rashad','darro','antonia','chas']
       
       			names2=['cyril','ronny','stevie','lon','freeman','erin','duncan','kennith','carmine','augustine','young','chadwick','wilburn','jonas','lazaro','brooks','ariel','dusty','tracey','scottie','seymour']
       			firstname=random.choice(names1)
       			lastname=random.choice(names2)
       			contactUID= j*1000+1234
       			PhoneNumber= str(random.randint(0, sys.maxint))
       			sparql_insert=INSERT_SPARQL % (contact,firstname,lastname, contactUID,PhoneNumber)
       			a=a+sparql_insert
     		INSERT= "INSERT{" + a + "}"

       		self.resources.BatchSparqlUpdate(INSERT)

		""" querry no. of existing contacts. """
		result = self.query ('SELECT ?c ?Fname ?Gname ?number WHERE { \
					?c a  nco:PersonContact ; \
					nco:nameGiven ?Gname ; \
					nco:nameFamily ?Fname; \
					nco:hasPhoneNumber ?number. \
					} LIMIT 1000')

		count_after_insert = len(result)
		print "contact count after insert %d" %count_after_insert

		""" cleanup the inserted contacts """
     		for j in range(100) :
			uid = j*1000+1234
			delete = 'DELETE {?contact a rdfs:Resource} WHERE {?contact nco:contactUID  <%s> }' %uid
			self.sparql_update (delete)

		"""test verification """
		if count_after_insert == 1000:
			if count_after_insert >= count_before_insert:
                		self.assert_(True,'Pass')
			else:
                		self.fail('batch insertion failed')
		else:
			if count_after_insert == count_before_insert + 100:
                		self.assert_(True,'Pass')
			else:
                		self.fail('batch insertion failed')
			

class phone_no (TestUpdate):
	def test_phone_01 (self):
		"""1. Setting the maemo:localPhoneNumber property to last 7 digits of phone number.
		   2. Receiving a message  from a contact whose localPhoneNumber is saved.
		   3. Querying for the local phone number.
		"""   
		PhoneNumber = str(random.randint (0, sys.maxint))
		UUID	    = str(time.time())
		UUID1	    = str(random.randint (0, sys.maxint))
		UUID2	    = str(random.randint (0, sys.maxint))
		localNumber = PhoneNumber[-7:]
		d=datetime.datetime.now()
	        Received=d.isoformat()
		ID	    = int(time.time())%1000
		Given_Name  = 'test_GN_' + `ID`
		Family_Name = 'test_FN_' + `ID`

		INSERT_SPARQL = """ INSERT { 
				<tel:%s> a nco:PhoneNumber ; 
				nco:phoneNumber  '%s' .
				<urn:uuid:%s> a nco:PersonContact;
				nco:contactUID <contact:test_%s>;
				nco:nameFamily '%s'  ; 
				nco:nameGiven '%s'.  
				<urn:uuid:%s>  nco:hasPhoneNumber <tel:%s>. 
				<tel:%s> maemo:localPhoneNumber '%s' 
				}"""
		sparql_insert = INSERT_SPARQL % (PhoneNumber,PhoneNumber,UUID,UUID1,Given_Name,Family_Name,UUID,PhoneNumber,PhoneNumber,localNumber)               
		try :
	            self.resources.SparqlUpdate(sparql_insert)
		except :
		    self.fail('Insertion is not successful')			

		INSERT_SPARQL1 = """ INSERT { 
				 <urn:uuid:%s> a nmo:Message ; 
				 nmo:from [a nco:Contact ; 
				 nco:hasPhoneNumber <tel:%s>];
			 	 nmo:receivedDate '%s' ; 
				 nmo:plainTextMessageContent 'hello' }"""

		sparql_insert = INSERT_SPARQL1 % ( UUID2,PhoneNumber,Received)
		try :
	            self.resources.SparqlUpdate(sparql_insert)
		except :
		    self.fail('Insertion is not successful')			

		QUERY_SPARQL = """ SELECT  ?local 
				WHERE { ?msg a nmo:Message .
				?c a nco:Contact;
				nco:hasPhoneNumber <tel:%s>.
				<tel:%s> maemo:localPhoneNumber ?local

				} """
		QUERY= QUERY_SPARQL %(PhoneNumber,PhoneNumber)
                result = self.resources.SparqlQuery(QUERY) 
		self.assert_(result[0][0].find(localNumber)!=-1 , 'Query is not succesful')

	def test_phone_02 (self):

		""" Inserting a local phone number which have spaces """

		INSERT_SPARQL = """ INSERT { 
				<tel+3333333333> a nco:PhoneNumber ; 
				nco:phoneNumber  <tel+3333333333> . 
				<urn:uuid:9876> a nco:PersonContact;
				nco:nameFamily 'test_name_01' ; 
				nco:nameGiven 'test_name_02'.  
				<urn:uuid:98765>  nco:hasPhoneNumber <tel+3333333333>. 
				<tel+3333333333> maemo:localPhoneNumber <333 333> }"""

		try:
		  self.resources.SparqlUpdate(INSERT_SPARQL)
                except :                                                                                                     
                   print "error in query execution"                                                                     
                   self.assert_(True,'error in query execution')                                                        
                else:                                                                                                        
                    self.fail('Query successfully executed')



	
if __name__ == "__main__":
	unittest.main()
