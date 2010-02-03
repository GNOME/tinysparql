#!/usr/bin/python
#-*- coding: latin-1 -*-


import sys,os,dbus,commands, signal, re
import unittest
import pickle
import configuration

testDataDir = configuration.dir_path()
print testDataDir
testDataDir_Music = testDataDir+'Music'
target = configuration.check_target()
print target

TRACKER = 'org.freedesktop.Tracker1'
TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources'
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources"

if target == '1': 
	MUSIC_FILE_PATH = "file:///home/user/MyDocs/.sounds/"
else:
	MUSIC_FILE_PATH = "file:///home/user/MyDocs/.sounds/"
	#MUSIC_FILE_PATH = "file:///usr/share/tracker-tests/data/Music/"

class TrackerHelpers(unittest.TestCase):	
	
	
	def setUp(self):
		bus = dbus.SessionBus()
		tracker = bus.get_object(TRACKER, TRACKER_OBJ)
	        self.resources = dbus.Interface (tracker,
	                                         dbus_interface=RESOURCES_IFACE)


	def de_pickle(self,pckl_file):
		if target == '1': 
			pckl_file = '/usr/share/tracker-tests/data/'+pckl_file
		else:
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


class images(TrackerHelpers):

	def test_get_images_height_1(self):
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
		dictList = self.de_pickle('pickled_Music')
		print dictList
		overallRes = []
                for adict in dictList:
			Results = {}
                        testFile = adict['FILENAME']
			#file_uri = "file://" + testFile
			file_uri = MUSIC_FILE_PATH + testFile
			print 'testfile is %s' %testFile
                        for parm, expRes in adict.iteritems():
				print parm
                                if parm.rstrip() == 'title':
					print parm
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


unittest.main()

