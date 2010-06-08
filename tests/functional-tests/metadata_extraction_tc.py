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

if target == configuration.MAEMO6_HW:
	"""target is device """
	IMAGE_FILE_PATH = configuration.URL_PREFIX + configuration.MYDOCS_IMAGES
	MUSIC_FILE_PATH = configuration.URL_PREFIX + configuration.MYDOCS_MUSIC
	PCKL_FILE_PATH = configuration.TEST_DATA_DIR

elif target == configuration.DESKTOP:
	"""target is DESKTOP """
	IMAGE_FILE_PATH = configuration.URL_PREFIX + os.path.expanduser("~") + '/'
	MUSIC_FILE_PATH = configuration.URL_PREFIX + os.path.expanduser("~") + '/'
	PCKL_FILE_PATH = configuration.VCS_TEST_DATA_DIR

print "MUSIC_FILE_PATH is %s" %(MUSIC_FILE_PATH)

tdcpy = configuration.TDCopy()
tdcpy.set_test_data(target)


class TrackerHelpers(unittest.TestCase):
	def setUp(self):
		bus = dbus.SessionBus()
		tracker = bus.get_object(TRACKER, TRACKER_OBJ)
	        self.resources = dbus.Interface (tracker,
	                                         dbus_interface=RESOURCES_IFACE)


	def de_pickle(self,pckl_file):
		pckl_file =  PCKL_FILE_PATH + pckl_file
		print "pickle file is %s" %(pckl_file)
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


class images(TrackerHelpers):

	def test_get_images_height_1(self):
		"""
		get the height of the image files
		and verify them with that present in the earlier created dictionary
		"""
		dictList = self.de_pickle('pickled_Images')
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			print 'testfile is %s' %testFile
			file_uri = IMAGE_FILE_PATH + testFile
			"""browse the file's metadata list in dictionary for height """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Image Height':
					query = "SELECT ?height WHERE { \
					?uid nie:url <%s>; \
					nfo:height ?height.}" %(file_uri)
					print query
					results = self.query (query)
					print results
					print  'Actual = ' + results[0][0]
					print  'Expected = ' + expRes.strip()
					if  not expRes.strip() == results[0][0]:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct height for file %s' %testFile
					else:
						flag = True
			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Image files %s\n\n' % (str(overallRes)) )

	def test_get_images_width_1(self):
		"""
		get the width of the image files
		and verify them with that present in the earlier created dictionary
		"""
		dictList = self.de_pickle('pickled_Images')
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			print 'testfile is %s' %testFile
			file_uri = IMAGE_FILE_PATH + testFile
			"""browse the file's metadata list in dictionary for width """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Image Width':
					query = "SELECT ?value WHERE { \
					?uid nie:url <%s>; \
					nfo:width ?value.}" %(file_uri)
					print query
					results = self.query (query)
					print results
					print  'Actual = ' + results[0][0]
					print  'Expected = ' + expRes.strip()
					if  not expRes.strip() == results[0][0]:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct width for file %s' %testFile
					else:
						flag = True
			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Image files %s\n\n' % (str(overallRes)) )


	def test_get_images_title_1(self):
		"""
		get the title of the image files
		and verify them with that present in the earlier created dictionary
		"""
		dictList = self.de_pickle('pickled_Images')
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			print 'testfile is %s' %testFile
			file_uri = IMAGE_FILE_PATH + testFile
			"""browse the file's metadata list in dictionary for title """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Title':
					query = "SELECT ?value WHERE { \
					?uid nie:url <%s>; \
					nie:title ?value.}" %(file_uri)
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
				self.assert_(not k,'Get Metadata failed for following Image files %s\n\n' % (str(overallRes)) )


	def test_get_images_creator_1(self):
		"""
		get the creator of the image files
		and verify them with that present in the earlier created dictionary
		"""
		dictList = self.de_pickle('pickled_Images')
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			print 'testfile is %s' %testFile
			file_uri = IMAGE_FILE_PATH + testFile
			"""browse the file's metadata list in dictionary for creator"""
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Creator':
					query = "SELECT ?value WHERE { \
					?uid nie:url <%s>; \
					nco:creator ?urn. \
					?urn nco:fullname ?value}" %(file_uri)
					print query
					results = self.query (query)
					print results
					print  'Actual = ' + results[0][0]
					print  'Expected = ' + expRes.strip()
					if  not expRes.strip() == results[0][0]:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct creator for file %s' %testFile
					else:
						flag = True
			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Image files %s\n\n' % (str(overallRes)) )

	def test_get_images_mime_1(self):
		"""
		get the mime of the image files
		and verify them with that present in the earlier created dictionary
		"""
		dictList = self.de_pickle('pickled_Images')
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			print 'testfile is %s' %testFile
			file_uri = IMAGE_FILE_PATH + testFile
			"""browse the file's metadata list in dictionary for mime"""
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'MIME Type':
					query = "SELECT ?value WHERE { \
					?uid nie:url <%s>; \
					nie:mimeType ?value.}" %(file_uri)
					print query
					results = self.query (query)
					print results
					print  'Actual = ' + results[0][0]
					print  'Expected = ' + expRes.strip()
					if  not expRes.strip() == results[0][0]:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct mime for file %s' %testFile
					else:
						flag = True
			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Image files %s\n\n' % (str(overallRes)) )


	def test_get_images_country_1(self):
		"""
		get the country of the image files
		and verify them with that present in the earlier created dictionary
		"""
		dictList = self.de_pickle('pickled_Images')
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			print 'testfile is %s' %testFile
			file_uri = IMAGE_FILE_PATH + testFile
			"""browse the file's metadata list in dictionary for country"""
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Country':
					query = "SELECT ?value WHERE { \
					?uid nie:url <%s>; \
					mlo:location ?urn. \
					?urn mlo:country ?value}" %(file_uri)
					print query
					results = self.query (query)
					print results
					print  'Actual = ' + results[0][0]
					print  'Expected = ' + expRes.strip()
					if  not expRes.strip() == results[0][0]:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct country for file %s' %testFile
					else:
						flag = True
			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Image files %s\n\n' % (str(overallRes)) )

	def test_get_images_city_1(self):
		"""
		get the city of the image files
		and verify them with that present in the earlier created dictionary
		"""
		dictList = self.de_pickle('pickled_Images')
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			print 'testfile is %s' %testFile
			file_uri = IMAGE_FILE_PATH + testFile
			"""browse the file's metadata list in dictionary for city"""
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'City':
					print "amit"
					query = "SELECT ?value WHERE { \
					?uid nie:url <%s>; \
					mlo:location ?urn. \
					?urn mlo:city ?value}" %(file_uri)
					print query
					results = self.query (query)
					print results
					print  'Actual = ' + results[0][0]
					print  'Expected = ' + expRes.strip()
					if  not expRes.strip() == results[0][0]:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct city for file %s' %testFile
					else:
						flag = True
			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Image files %s\n\n' % (str(overallRes)) )



	def test_get_images_res_1(self):
		"""
		get the X Resolution of the image files
		and verify them with that present in the earlier created dictionary
		"""
		dictList = self.de_pickle('pickled_Images')
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			print 'testfile is %s' %testFile
			file_uri = IMAGE_FILE_PATH + testFile
			"""browse the file's metadata list in dictionary for X Resolution """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'X Resolution':
					query = "SELECT ?value WHERE { \
					?uid nie:url <%s>; \
					nfo:horizontalResolution ?value}" %(file_uri)
					print query
					results = self.query (query)
					print results
					print  'Expected = ' + expRes.strip()
					if len(results) > 0 and expRes.strip() == results[0][0]:
						print  'Actual = ' + results[0][0]
						flag = True
					else:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct X Resolution  for file %s' %testFile

			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Image files %s\n\n' % (str(overallRes)) )

	def test_get_images_res_2(self):
		"""
		get the Y Resolution of the image files
		and verify them with that present in the earlier created dictionary
		"""
		dictList = self.de_pickle('pickled_Images')
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			print 'testfile is %s' %testFile
			file_uri = IMAGE_FILE_PATH + testFile
			"""browse the file's metadata list in dictionary for Y Resolution """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Y Resolution':
					query = "SELECT ?value WHERE { \
					?uid nie:url <%s>; \
					nfo:verticalResolution ?value}" %(file_uri)
					print query
					results = self.query (query)
					print results
					print  'Expected = ' + expRes.strip()
					if len(results) > 0 and expRes.strip() == results[0][0]:
						print  'Actual = ' + results[0][0]
						flag = True
					else:
						Results[testFile]=parm
						flag = False
						print 'Failed to get correct Y Resolution  for file %s' %testFile

			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Image files %s\n\n' % (str(overallRes)) )

	def test_get_images_copyright_1(self):
		"""
		get the copyright of the image files
		and verify them with that present in the earlier created dictionary
		"""
		dictList = self.de_pickle('pickled_Images')
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			print 'testfile is %s' %testFile
			file_uri = IMAGE_FILE_PATH + testFile
			"""browse the file's metadata list in dictionary for copyright """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Copyright':
					query = "SELECT ?value WHERE { \
					?uid nie:url <%s>; \
					nie:copyright ?value.}" %(file_uri)
					print query
					results = self.query (query)
					print results
					print  'Expected = ' + expRes.strip()
					if len(results) > 0 and expRes.strip() == results[0][0]:
						print  'Actual = ' + results[0][0]
					else:
						Results[testFile]=parm
						print 'Failed to get correct copyright  for file %s' %testFile

			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Image files %s\n\n' % (str(overallRes)) )

	def test_get_images_fnumber_1(self):
		"""
		get the fnumber of the image files
		and verify them with that present in the earlier created dictionary
		"""
		dictList = self.de_pickle('pickled_Images')
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			print 'testfile is %s' %testFile
			file_uri = IMAGE_FILE_PATH + testFile
			"""browse the file's metadata list in dictionary for fnumber """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'F Number':
					query = "SELECT ?value WHERE { \
					?uid nie:url <%s>; \
					nmm:fnumber ?value.}" %(file_uri)
					print query
					results = self.query (query)
					print results
					print  'Expected = ' + expRes.strip()
					if len(results) > 0 and expRes.strip() == results[0][0]:
						print  'Actual = ' + results[0][0]
					else:
						Results[testFile]=parm
						print 'Failed to get correct fnumber  for file %s' %testFile

			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Image files %s\n\n' % (str(overallRes)) )

	def test_get_images_focal_length_1(self):
		"""
		get the focal_length of the image files
		and verify them with that present in the earlier created dictionary
		"""
		dictList = self.de_pickle('pickled_Images')
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			print 'testfile is %s' %testFile
			file_uri = IMAGE_FILE_PATH + testFile
			"""browse the file's metadata list in dictionary for focal_length """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Focal Length':
					query = "SELECT ?value WHERE { \
					?uid nie:url <%s>; \
					nmm:focalLength ?value.}" %(file_uri)
					print query
					results = self.query (query)
					print results
					print  'Expected = ' + expRes.strip()
					exp_val = (expRes.strip()).split(' mm')
					if len(results) > 0 and exp_val[0] == results[0][0]:
						print  'Actual = ' + results[0][0]
					else:
						Results[testFile]=parm
						print 'Failed to get correct focal_length  for file %s' %testFile

			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Image files %s\n\n' % (str(overallRes)) )

	def test_get_images_keyword_1(self):
		"""
		get the keyword of the image files
		and verify them with that present in the earlier created dictionary
		"""
		dictList = self.de_pickle('pickled_Images')
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			print 'testfile is %s' %testFile
			file_uri = IMAGE_FILE_PATH + testFile
			"""browse the file's metadata list in dictionary for keyword """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Keyword':
					query = "SELECT ?value WHERE { \
					?i a nfo:Image .\
					?i nie:url <%s>.\
					?i nao:hasTag ?tag.\
					?tag nao:prefLabel ?value.}" %(file_uri)
					print query
					results = self.query (query)
					print results
					print  'Expected = ' + expRes.strip()
					if len(results) > 0 and expRes.strip() == results[0][0]:
						print  'Actual = ' + results[0][0]
					else:
						Results[testFile]=parm
						print 'Failed to get correct keyword  for file %s' %testFile

			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Image files %s\n\n' % (str(overallRes)) )

	def test_get_images_comment_1(self):
		"""
		get the comment of the image files
		and verify them with that present in the earlier created dictionary
		"""
		dictList = self.de_pickle('pickled_Images')
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			print 'testfile is %s' %testFile
			file_uri = IMAGE_FILE_PATH + testFile
			"""browse the file's metadata list in dictionary for comment """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'comment':
					query = "SELECT ?value WHERE { \
					?uid nie:url <%s>; \
					nie:comment ?value.}" %(file_uri)
					print query
					results = self.query (query)
					print results
					print  'Expected = ' + expRes.strip()
					if len(results) > 0 and expRes.strip() == results[0][0]:
						print  'Actual = ' + results[0][0]
					else:
						Results[testFile]=parm
						print 'Failed to get correct comment  for file %s' %testFile

			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Image files %s\n\n' % (str(overallRes)) )

	def test_get_images_camera_1(self):
		"""
		get the camera of the image files
		and verify them with that present in the earlier created dictionary
		TODO: Have a image file with camera property.
		"""
		dictList = self.de_pickle('pickled_Images')
		overallRes = []
                for adict in dictList:
			"""adict is a file"""
			Results = {}
                        testFile = adict['FILENAME']
			print 'testfile is %s' %testFile
			file_uri = IMAGE_FILE_PATH + testFile
			"""browse the file's metadata list in dictionary for camera """
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'Camera':
					query = "SELECT ?value WHERE { \
					?uid nie:url <%s>; \
					nfo:device ?value.}" %(file_uri)
					print query
					results = self.query (query)
					print results
					print  'Expected = ' + expRes.strip()
					if len(results) > 0 and expRes.strip() == results[0][0]:
						print  'Actual = ' + results[0][0]
					else:
						Results[testFile]=parm
						print 'Failed to get correct camera  for file %s' %testFile

			overallRes.append(Results)
		for Result_dict in overallRes:
			for k in Result_dict:
				self.assert_(not k,'Get Metadata failed for following Image files %s\n\n' % (str(overallRes)) )





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
					?uid a nfo:FileDataObject; \
					nie:url <%s>; \
					nie:title ?title.}" %(file_uri)
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
					?uid a nfo:FileDataObject; \
					nie:url <%s>; \
					nfo:genre ?genre.}" %(file_uri)
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
					?uid a nfo:FileDataObject; \
					nie:url <%s>; \
					nmm:composer ?composer.}" %(file_uri)
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
					?uid a nfo:FileDataObject; \
					nie:url <%s>; \
					nmm:performer ?artist.}" %(file_uri)
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
					?uid a nfo:FileDataObject; \
					nie:url <%s>; \
					nmm:musicAlbum ?album.}" %(file_uri)
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
					?uid a nfo:FileDataObject; \
					nie:url <%s>; \
					nie:copyright ?copyright.}" %(file_uri)
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
					?uid a nfo:FileDataObject; \
					nie:url <%s>; \
					nmm:trackNumber ?track.}" %(file_uri)
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
					?uid a nfo:FileDataObject; \
					nie:url <%s>; \
					nie:contentCreated ?date.}" %(file_uri)
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
					?uid a nfo:FileDataObject; \
					nie:url <%s>; \
					nie:mimeType ?mime.}" %(file_uri)
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
					?uid a nfo:FileDataObject; \
					nie:url <%s>; \
					nco:contributor ?contributor.}" %(file_uri)
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
					?uid a nfo:FileDataObject; \
					nie:url <%s>; \
					nfo:duration ?duration.}" %(file_uri)
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
					?uid a nfo:FileDataObject; \
					nie:url <%s>; \
					nie:comment ?comment.}" %(file_uri)
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


if __name__ == "__main__":
	unittest.main()

