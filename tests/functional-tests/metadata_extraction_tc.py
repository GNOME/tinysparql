#!/usr/bin/env python2.5

# Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA  02110-1301, USA.
#


import sys,os,dbus,commands, signal, re
import unittest
import pickle
import configuration

""" get target environment """
target = configuration.check_target()
print target

TRACKER = 'org.freedesktop.Tracker1'
TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources'
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources"

if target == '2':
	"""target is device """
	MUSIC_FILE_PATH = "file:///home/user/MyDocs/.sounds/"
else:
	"""target is SBOX """
	MUSIC_FILE_PATH = "file:///usr/share/tracker-tests/data/Music/"

class TrackerHelpers(unittest.TestCase):
	def setUp(self):
		bus = dbus.SessionBus()
		tracker = bus.get_object(TRACKER, TRACKER_OBJ)
	        self.resources = dbus.Interface (tracker,
	                                         dbus_interface=RESOURCES_IFACE)


	def de_pickle(self,pckl_file):
		pckl_file = '/usr/share/tracker-tests/data/'+pckl_file
		pickf=open(pckl_file, 'rb')
		dictList=pickle.load(pickf)
		pickf.close()
		print dictList
		return dictList

	def sparql_update(self,query):
                return self.resources.SparqlUpdate(query)

        def query(self,query):
                return self.resources.SparqlQuery(query)

class mydict:
  def __init__ (self):
    self.nested_dict = { }
  def append (self, key, value):
    if not self.nested_dict.has_key (key):
      self.nested_dict[key] = { }
    self.nested_dict[key][value] = 1
  def __getitem__ (self, key):
    return self.nested_dict[key].keys ()


#class images(TrackerHelpers):

	#def test_get_images_height_1(self):
		#dictList = self.de_pickle('pickled_Images')
		#print dictList
		#item = 'Image Height'
		#flag = False
                #for adict in dictList:
		#	# adict is a file
                #        testFile = adict['FILENAME']
		#	file_uri = "file://" + testFile
		#	#file_uri = MUSIC_FILE_PATH + testFile
		#	print 'testfile is %s' %file_uri
                #        for parm, expRes in adict.iteritems():
		#		# iterate thro the dictaionary, file's fields
		##		if re.compile('^'+item+'$',re.M).search(parm):
		#			print 'parm val is %s' %parm
		#			query = "SELECT ?height WHERE { \
		#			<%s> a nfo:FileDataObject; \
		#			nfo:height ?height.}" %(file_uri)
		#			print query
		#			results = self.query (query)
		#			print results
		##
		#			print 'height of image retrieved is %d' (results[0][0])
		#			print 'value in dic %s' %(expRes.strip())
		#			if  not expRes.strip() == height:
		#				flag = False
		#				print 'Failed to get correct height of image %s' %testFile
		#			else:
		#				flag = True

		#self.assert_(flag, "Get metadata for Images failed." )

class music(TrackerHelpers):

	def test_get_music_title_1(self):
		"""
		get the title of the music files
		and verify them with that present in the earlier created dictionary
		"""
		dictList = self.de_pickle('pickled_Music')
		print dictList
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			print 'testfile is %s' %testFile
			file_uri = MUSIC_FILE_PATH + testFile
			"""browse the file's metadata list in dictionary for title """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Title':
					query = "SELECT ?title WHERE { \
					<%s> a nfo:FileDataObject. \
					<%s> nie:title ?title.}" %(file_uri,file_uri)
					print query
					results = self.query (query)
					print results
					print  'Actual = ' + results[0][0]
					print  'Expected = ' + expRes.strip()
					if  not expRes.strip() == results[0][0]:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct title for file %s' %testFile
					else:
						flag = True
			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Music files %s\n\n' % (str(overallRes)) )


	def test_get_music_genre_1(self):

		"""
		get the genre of the music files
		and verify them with that present in the earlier created dictionary
		"""

		dictList = self.de_pickle('pickled_Music')
		print dictList
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			file_uri = MUSIC_FILE_PATH + testFile
			print 'testfile is %s' %testFile

			"""browse the file's metadata list in dictionary for genre """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Genre':
					print parm
					query = "SELECT ?genre WHERE { \
					<%s> a nfo:FileDataObject. \
					<%s> nfo:genre ?genre.}" %(file_uri,file_uri)
					print query
					results = self.query (query)
					print results
					print  'Actual = ' + results[0][0]
					print  'Expected = ' + expRes.strip()
					if  not expRes.strip() == results[0][0]:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct genre for file %s' %testFile
					else:
						flag = True
			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Music files %s\n\n' % (str(overallRes)) )


	def test_get_music_composer_1(self):

		"""
		get the copmpser of the music files
		and verify them with that present in the earlier created dictionary
		"""

		dictList = self.de_pickle('pickled_Music')
		print dictList
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			file_uri = MUSIC_FILE_PATH + testFile
			print 'testfile is %s' %testFile

			"""browse the file's metadata list in dictionary for creator """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Composer':
					print parm
					query = "SELECT ?composer WHERE { \
					<%s> a nfo:FileDataObject. \
					<%s> nmm:composer ?composer.}" %(file_uri,file_uri)
					print query
					results = self.query (query)
					print results
					value = results[0][0].split('urn:artist:')
					print  'Actual = ' + value[1]
					print  'Expected = ' + expRes.strip()
					if  not expRes.strip() == value[1]:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct composer for file %s' %testFile
					else:
						flag = True
			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Music files %s\n\n' % (str(overallRes)) )


	def test_get_music_performer_1(self):

		"""
		get the performer of the music files
		and verify them with that present in the earlier created dictionary
		"""

		dictList = self.de_pickle('pickled_Music')
		print dictList
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			file_uri = MUSIC_FILE_PATH + testFile
			print 'testfile is %s' %testFile

			"""browse the file's metadata list in dictionary for performer """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Artist':
					print parm
					query = "SELECT ?artist WHERE { \
					<%s> a nfo:FileDataObject. \
					<%s> nmm:performer ?artist.}" %(file_uri,file_uri)
					print query
					results = self.query (query)
					print results
					value = results[0][0].split('urn:artist:')
					print  'Actual = ' + value[1]
					print  'Expected = ' + expRes.strip()
					if  not expRes.strip() == value[1]:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct performer for file %s' %testFile
					else:
						flag = True
			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Music files %s\n\n' % (str(overallRes)) )


	def test_get_music_album_1(self):

		"""
		get the music album of the music files
		and verify them with that present in the earlier created dictionary
		"""

		dictList = self.de_pickle('pickled_Music')
		print dictList
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			file_uri = MUSIC_FILE_PATH + testFile
			print 'testfile is %s' %testFile

			"""browse the file's metadata list in dictionary for music album """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Album':
					print parm
					query = "SELECT ?album WHERE { \
					<%s> a nfo:FileDataObject. \
					<%s> nmm:musicAlbum ?album.}" %(file_uri,file_uri)
					print query
					results = self.query (query)
					print results
					value = results[0][0].split('urn:album:')
					print  'Actual = ' + value[1]
					print  'Expected = ' + expRes.strip()
					if  not expRes.strip() == value[1]:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct album for file %s' %testFile
					else:
						flag = True
			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Music files %s\n\n' % (str(overallRes)) )


	def test_get_music_copyright_1(self):

		"""
		get the copyright of the music files
		and verify them with that present in the earlier created dictionary
		"""

		dictList = self.de_pickle('pickled_Music')
		print dictList
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			file_uri = MUSIC_FILE_PATH + testFile
			print 'testfile is %s' %testFile

			"""browse the file's metadata list in dictionary for copyright """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Copyright':
					print parm
					query = "SELECT ?copyright WHERE { \
					<%s> a nfo:FileDataObject. \
					<%s> nie:copyright ?copyright.}" %(file_uri,file_uri)
					print query
					results = self.query (query)
					print results
					print  'Actual = ' + results[0][0]
					print  'Expected = ' + expRes.strip()
					if  not expRes.strip() == results[0][0]:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct copyright for file %s' %testFile
					else:
						flag = True
			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Music files %s\n\n' % (str(overallRes)) )


	def test_get_music_track_1(self):

		"""
		get the track number of the music files
		and verify them with that present in the earlier created dictionary
		"""

		dictList = self.de_pickle('pickled_Music')
		print dictList
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			file_uri = MUSIC_FILE_PATH + testFile
			print 'testfile is %s' %testFile

			"""browse the file's metadata list in dictionary for track number """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Track':
					print parm
					query = "SELECT ?track WHERE { \
					<%s> a nfo:FileDataObject. \
					<%s> nmm:trackNumber ?track.}" %(file_uri,file_uri)
					print query
					results = self.query (query)
					print results
					print  'Actual = ' + results[0][0]
					print  'Expected = ' + expRes.strip()
					if  not expRes.strip() == results[0][0]:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct track number for file %s' %testFile
					else:
						flag = True
			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Music files %s\n\n' % (str(overallRes)) )


	def ztest_get_music_date_1(self):

		"""
		get the creation date of the music files
		and verify them with that present in the earlier created dictionary
		"""

		dictList = self.de_pickle('pickled_Music')
		print dictList
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			file_uri = MUSIC_FILE_PATH + testFile
			print 'testfile is %s' %testFile

			"""browse the file's metadata list in dictionary for creation date """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'File Modification Date/Time':
					print parm
					query = "SELECT ?date WHERE { \
					<%s> a nfo:FileDataObject. \
					<%s> nie:contentCreated ?date.}" %(file_uri,file_uri)
					print query
					results = self.query (query)
					print results
					print  'Actual = ' + results[0][0]
					print  'Expected = ' + expRes.strip()
					if  not expRes.strip() == results[0][0]:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct creation date for file %s' %testFile
					else:
						flag = True
			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Music files %s\n\n' % (str(overallRes)) )


	def test_get_music_mime_1(self):

		"""
		get the mime type of the music files
		and verify them with that present in the earlier created dictionary
		"""

		dictList = self.de_pickle('pickled_Music')
		print dictList
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			file_uri = MUSIC_FILE_PATH + testFile
			print 'testfile is %s' %testFile

			"""browse the file's metadata list in dictionary for mime type """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'MIME Type':
					print parm
					query = "SELECT ?mime WHERE { \
					<%s> a nfo:FileDataObject. \
					<%s> nie:mimeType ?mime.}" %(file_uri,file_uri)
					print query
					results = self.query (query)
					print results
					print  'Actual = ' + results[0][0]
					print  'Expected = ' + expRes.strip()
					if  not expRes.strip() == results[0][0]:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct mime type for file %s' %testFile
					else:
						flag = True
			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Music files %s\n\n' % (str(overallRes)) )


	def test_get_music_contributor_1(self):

		"""
		get the contributor of the music files
		and verify them with that present in the earlier created dictionary
		"""

		dictList = self.de_pickle('pickled_Music')
		print dictList
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			file_uri = MUSIC_FILE_PATH + testFile
			print 'testfile is %s' %testFile

			"""browse the file's metadata list in dictionary for contributor """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'contributor':
					print parm
					query = "SELECT ?contributor WHERE { \
					<%s> a nfo:FileDataObject. \
					<%s> nco:contributor ?contributor.}" %(file_uri,file_uri)
					print query
					results = self.query (query)
					print results
					value = results[0][0].split('urn:artist:')
					print  'Actual = ' + value[1]
					print  'Expected = ' + expRes.strip()
					if  not expRes.strip() == value[1]:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct contributor for file %s' %testFile
					else:
						flag = True
			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Music files %s\n\n' % (str(overallRes)) )


	def test_get_music_duration_1(self):

		"""
		get the duration of the music files
		and verify them with that present in the earlier created dictionary
		"""

		dictList = self.de_pickle('pickled_Music')
		print dictList
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			file_uri = MUSIC_FILE_PATH + testFile
			print 'testfile is %s' %testFile

			"""browse the file's metadata list in dictionary for duration """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Duration':
					print parm
					query = "SELECT ?duration WHERE { \
					<%s> a nfo:FileDataObject. \
					<%s> nmm:length ?duration.}" %(file_uri,file_uri)
					print query
					results = self.query (query)
					print results
					exp_value = (expRes.strip()).split('.')
					print  'Actual = ' + results[0][0]
					print  'Expected = ' + exp_value[0]
					if  not exp_value[0] == results[0][0]:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct duration for file %s' %testFile
					else:
						flag = True
			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Music files %s\n\n' % (str(overallRes)) )


	def test_get_music_comment_1(self):

		"""
		get the comment of the music files
		and verify them with that present in the earlier created dictionary
		"""

		dictList = self.de_pickle('pickled_Music')
		print dictList
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			file_uri = MUSIC_FILE_PATH + testFile
			print 'testfile is %s' %testFile

			"""browse the file's metadata list in dictionary for comment """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Comment':
					print parm
					query = "SELECT ?comment WHERE { \
					<%s> a nfo:FileDataObject. \
					<%s> nie:comment ?comment.}" %(file_uri,file_uri)
					print query
					results = self.query (query)
					print results
					print  'Actual = ' + results[0][0]
					print  'Expected = ' + expRes.strip()
					if  not expRes.strip() == results[0][0]:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct comment for file %s' %testFile
					else:
						flag = True
			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Music files %s\n\n' % (str(overallRes)) )


unittest.main()
