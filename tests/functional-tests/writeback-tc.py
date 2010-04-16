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

import sys,os,dbus
import unittest
import time
import random
import configuration
import commands
import signal

TRACKER = 'org.freedesktop.Tracker1'
TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources'
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources"

target = configuration.check_target()
print target

if target == configuration.MAEMO6_HW:
	"""target is device """
	tracker_wb = configuration.TRACKER_WRITEBACK
	tracker_ext = configuration.TRACKER_EXTRACT
	dir_path = configuration.WB_TEST_DIR_DEVICE

elif target == configuration.DESKTOP:
	tracker_wb = configuration.TRACKER_WRITEBACK_DESKTOP
	tracker_ext = configuration.TRACKER_EXTRACT_DESKTOP
	dir_path = configuration.WB_TEST_DIR_HOST

print dir_path

""" run the tracker-writeback """
pid = int(commands.getoutput('pidof tracker-writeback | wc -w'))

if not pid:
	os.system(tracker_wb + ' -v 3 &')
	time.sleep(5)

def copy_file():
	src = configuration.VCS_TEST_DATA_IMAGES + configuration.TEST_IMAGE
	dest = dir_path
	print 'Copying '+src+' to '+dest
	commands.getoutput('cp '+src + ' '+dest)

def create_file_list_in_dir() :
	"""
	1. create a test directory
	2. copy images to test directory
	3. list the files present in the test data directory
	"""

	commands.getoutput('mkdir ' + dir_path)

	copy_file()

        fileList = []
        dirpathList = []
        for dirpath,dirNames,fileNames in os.walk(dir_path):
		dirslist = os.listdir(dirpath)
       	        for filename in dirslist:
               	        fileList.append(dirpath+'/'+filename)

        return fileList

""" get the list of files present in the test data directory """
file_list = create_file_list_in_dir()


class TestWriteback (unittest.TestCase):

        def setUp(self):
                bus = dbus.SessionBus()
                tracker = bus.get_object(TRACKER, TRACKER_OBJ)
                self.resources = dbus.Interface (tracker,
                                                 dbus_interface=RESOURCES_IFACE)


        def sparql_update(self,query):
                return self.resources.SparqlUpdate(query)

        def query(self,query):
                return self.resources.SparqlQuery(query)


""" Writeback test cases """
class writeback(TestWriteback):

	def test_wb_01(self):
		"""
		Test if the value for Description property is written to the file
		with sparql update with writeback.

                1. Delete the value of the property of the file.
                2. Insert a new value.
                3. Verify the value is written to the file.
                4. Run the instruction in for loop to test for all the files
                present in a particular directory.
		"""

		"""create list to catch unupdated file """
		overallRes = []

		for i in range(len(file_list)) :
			"""browse through each file """
			uri = 'file://'+file_list[i]
			print uri
			Results = {}

                	insert = """
				DELETE {?file nie:description ?val }
				WHERE { ?file nie:description ?val ;nie:url <%s> }
				INSERT {?file nie:description 'testdescription' }
				WHERE { ?file nie:url <%s> }
                		""" %(uri, uri)
			print insert
                	self.sparql_update (insert)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nie:description')
			print result
                	value=result.split()
			if (result.find('nie:description')!=-1)  and (value[1]=='"testdescription"') :
				print "property is set"
                	else:
				print "property is NOT set"
				Results[uri]='nie:description'

			overallRes.append(Results)
                for Result_dict in overallRes:
                        for k in Result_dict:
                                self.assert_(not k,'Writeback failed for following files for property %s\n\n' % (str(overallRes)) )

			"""
                	self.assert_(result.find('nie:description')!=-1  and value[1]=='"testdescription"' , "Title is not updated for File: %s" %(uri))
                	if value[1]=='"testdescription"' :
				print "title is found"
                	else:
				print "title is NOT found"
			"""

	def test_wb_02(self):
		"""
		Test if the value for copyright property is written to the file
		with sparql update with writeback.

                1. Delete the value of the property of the file.
                2. Insert a new value.
                3. Verify the value is written to the file.
                4. Run the instruction in for loop to test for all the files
                present in a particular directory.
		"""

		"""create list to catch unupdated file """
		overallRes = []

		for i in range(len(file_list)) :
			"""browse through each file """
			uri = 'file://'+file_list[i]
			print uri
			Results = {}

                	insert = """
				DELETE {?file nie:copyright ?val }
				WHERE { ?file nie:copyright ?val ;nie:url <%s> }
				INSERT {?file nie:copyright 'testcopyright' }
				WHERE { ?file nie:url <%s> }
                		""" %(uri, uri)
			print insert
                	self.sparql_update (insert)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nie:copyright')
			print result
                	value=result.split()
			if (result.find('nie:copyright')!=-1)  and (value[1]=='"testcopyright"') :
				print "property is set"
                	else:
				print "property is NOT set"
				Results[uri]='nie:copyright'

			overallRes.append(Results)
                for Result_dict in overallRes:
                        for k in Result_dict:
                                self.assert_(not k,'Writeback failed for following files for property %s\n\n' % (str(overallRes)) )


	def test_wb_03(self):
		"""
		Test if the value for title property is written to the file
		with sparql update with writeback.

                1. Delete the value of the property of the file.
                2. Insert a new value.
                3. Verify the value is written to the file.
                4. Run the instruction in for loop to test for all the files
                present in a particular directory.
		"""

		"""create list to catch unupdated file """
		overallRes = []

		for i in range(len(file_list)) :
			"""browse through each file """
			uri = 'file://'+file_list[i]
			print uri
			Results = {}

                	insert = """
				DELETE {?file nie:title ?val }
				WHERE { ?file nie:title ?val ;nie:url <%s> }
				INSERT {?file nie:title 'testtitle' }
				WHERE { ?file nie:url <%s> }
                		""" %(uri, uri)
			print insert
                	self.sparql_update (insert)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nie:title')
			print result
                	value=result.split()
			if (result.find('nie:title')!=-1)  and (value[1]=='"testtitle"') :
				print "property is set"
                	else:
				print "property is NOT set"
				Results[uri]='nie:title'

			overallRes.append(Results)
                for Result_dict in overallRes:
                        for k in Result_dict:
                                self.assert_(not k,'Writeback failed for following files for property %s\n\n' % (str(overallRes)) )


	def test_wb_04(self):
		"""
		Test if the value for contentCreated property is written to the file
		with sparql update with writeback.

                1. Delete the value of the property of the file.
                2. Insert a new value.
                3. Verify the value is written to the file.
                4. Run the instruction in for loop to test for all the files
                present in a particular directory.
		"""

		"""create list to catch unupdated file """
		overallRes = []

		for i in range(len(file_list)) :
			"""browse through each file """
			uri = 'file://'+file_list[i]
			print uri
			Results = {}

                	insert = """
				DELETE {?file nie:contentCreated ?val }
				WHERE { ?file nie:contentCreated ?val ;nie:url <%s> }
				INSERT {?file nie:contentCreated '2004-05-06T13:14:15Z' }
				WHERE { ?file nie:url <%s> }
                		""" %(uri, uri)
			print insert
                	self.sparql_update (insert)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nie:contentCreated')
			print result
                	value=result.split()
			print value
			if (result.find('nie:contentCreated')!=-1)  and (value[1] == '2004-05-06T13:14:15Z') :
				print "property is set"
                	else:
				print "property is NOT set"
				Results[uri]='nie:contentCreated'

			overallRes.append(Results)
                for Result_dict in overallRes:
                        for k in Result_dict:
                                self.assert_(not k,'Writeback failed for following files for property %s\n\n' % (str(overallRes)) )


	def test_wb_05(self):
		"""
		Test if the value for keyword property is written to the file
		with sparql update with writeback.

                1. Delete the value of the property of the file.
                2. Insert a new value.
                3. Verify the value is written to the file.
                4. Run the instruction in for loop to test for all the files
                present in a particular directory.
		"""

		"""create list to catch unupdated file """
		overallRes = []

		for i in range(len(file_list)) :
			"""browse through each file """
			uri = 'file://'+file_list[i]
			print uri
			Results = {}

                	insert = """
				DELETE {?file nie:keyword ?val }
				WHERE { ?file nie:keyword ?val ;nie:url <%s> }
				INSERT {?file nie:keyword 'testkeyword' }
				WHERE { ?file nie:url <%s> }
                		""" %(uri, uri)
			print insert
                	self.sparql_update (insert)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nao:prefLabel')
			print result
                	value=result.split()
			if (result.find('nao:prefLabel')!=-1)  and ( '"testkeyword"' in value[1]) :
				print "property is set"
                	else:
				print "property is NOT set"
				Results[uri]='nie:keyword'

			overallRes.append(Results)
                for Result_dict in overallRes:
                        for k in Result_dict:
                                self.assert_(not k,'Writeback failed for following files for property %s\n\n' % (str(overallRes)) )


	def test_wb_06(self):
		"""
		Test if the value for contributor property is written to the file
		with sparql update with writeback.

                1. Delete the value of the property of the file.
                2. Insert a new value.
                3. Verify the value is written to the file.
                4. Run the instruction in for loop to test for all the files
                present in a particular directory.
		"""

		"""create list to catch unupdated file """
		overallRes = []

		for i in range(len(file_list)) :
			"""browse through each file """
			uri = 'file://'+file_list[i]
			print uri
			Results = {}

                	insert = """
				DELETE {?file nco:contributor ?val }
				WHERE { ?file nco:contributor ?val ;nie:url <%s> }
				INSERT {?file nco:contributor [a nco:Contact ; nco:fullname 'testcontributor' ] }
				WHERE { ?file nie:url <%s> }
                		""" %(uri, uri)
			print insert
                	self.sparql_update (insert)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nco:fullname')
			print result
			if '"testcontributor"' in result:
				print "property is set"
                	else:
				print "property is NOT set"
				Results[uri]='nco:contributor'

			overallRes.append(Results)
                for Result_dict in overallRes:
                        for k in Result_dict:
                                self.assert_(not k,'Writeback failed for following files for property %s\n\n' % (str(overallRes)) )

	def test_wb_07(self):
		"""
		Test if the value for creator property is written to the file
		with sparql update with writeback.

                1. Delete the value of the property of the file.
                2. Insert a new value.
                3. Verify the value is written to the file.
                4. Run the instruction in for loop to test for all the files
                present in a particular directory.
		"""

		"""create list to catch unupdated file """
		overallRes = []

		for i in range(len(file_list)) :
			"""browse through each file """
			uri = 'file://'+file_list[i]
			print uri
			Results = {}

                	insert = """
				DELETE {?file nco:creator ?val }
				WHERE { ?file nco:creator ?val ;nie:url <%s> }
				INSERT {?file nco:creator [a nco:Contact ; nco:fullname 'testcreator' ] }
				WHERE { ?file nie:url <%s> }
                		""" %(uri, uri)
			print insert
                	self.sparql_update (insert)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nco:fullname')
			print result
			if '"testcreator"' in result:
				print "property is set"
                	else:
				print "property is NOT set"
				Results[uri]='nco:creator'

			overallRes.append(Results)
                for Result_dict in overallRes:
                        for k in Result_dict:
                                self.assert_(not k,'Writeback failed for following files for property %s\n\n' % (str(overallRes)) )

	def test_wb_08(self):
		"""
		Test if the value for camera property is written to the file
		with sparql update with writeback.

                1. Delete the value of the property of the file.
                2. Insert a new value.
                3. Verify the value is written to the file.
                4. Run the instruction in for loop to test for all the files
                present in a particular directory.
		"""

		"""create list to catch unupdated file """
		overallRes = []

		for i in range(len(file_list)) :
			"""browse through each file """
			uri = 'file://'+file_list[i]
			print uri
			Results = {}

                	insert = """
				DELETE {?file nmm:camera ?val }
				WHERE { ?file nmm:camera ?val ;nie:url <%s> }
				INSERT {?file nmm:camera 'test camera' }
				WHERE { ?file nie:url <%s> }
                		""" %(uri, uri)
			print insert
                	self.sparql_update (insert)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nmm:camera')
			print result
                	value=result.split()
			if (result.find('nmm:camera')!=-1)  and (value[1]=='"test') and (value[2]=='camera"'):
				print "property is set"
                	else:
				print "property is NOT set"
				Results[uri]='nmm:camera'

			overallRes.append(Results)
                for Result_dict in overallRes:
                        for k in Result_dict:
                                self.assert_(not k,'Writeback failed for following files for property %s\n\n' % (str(overallRes)) )

	def test_wb_09(self):
		"""
		Test if the value for exposureTime property is written to the file
		with sparql update with writeback.

                1. Delete the value of the property of the file.
                2. Insert a new value.
                3. Verify the value is written to the file.
                4. Run the instruction in for loop to test for all the files
                present in a particular directory.
		"""

		"""create list to catch unupdated file """
		overallRes = []

		for i in range(len(file_list)) :
			"""browse through each file """
			uri = 'file://'+file_list[i]
			print uri
			Results = {}

                	insert = """
				DELETE {?file nmm:exposureTime ?val }
				WHERE { ?file nmm:exposureTime ?val ;nie:url <%s> }
				INSERT {?file nmm:exposureTime '44' }
				WHERE { ?file nie:url <%s> }
                		""" %(uri, uri)
			print insert
                	self.sparql_update (insert)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nmm:exposureTime')
			print result
                	value=result.split()
			if (result.find('nmm:exposureTime')!=-1)  and (value[1]=='44') :
				print "property is set"
                	else:
				print "property is NOT set"
				Results[uri]='nmm:exposureTime'

			overallRes.append(Results)
                for Result_dict in overallRes:
                        for k in Result_dict:
                                self.assert_(not k,'Writeback failed for following files for property %s\n\n' % (str(overallRes)) )

	def test_wb_10(self):
		"""
		Test if the value for fnumber property is written to the file
		with sparql update with writeback.

                1. Delete the value of the property of the file.
                2. Insert a new value.
                3. Verify the value is written to the file.
                4. Run the instruction in for loop to test for all the files
                present in a particular directory.
		"""

		"""create list to catch unupdated file """
		overallRes = []

		for i in range(len(file_list)) :
			"""browse through each file """
			uri = 'file://'+file_list[i]
			print uri
			Results = {}

                	insert = """
				DELETE {?file nmm:fnumber ?val }
				WHERE { ?file nmm:fnumber ?val ;nie:url <%s> }
				INSERT {?file nmm:fnumber '707' }
				WHERE { ?file nie:url <%s> }
                		""" %(uri, uri)
			print insert
                	self.sparql_update (insert)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nmm:fnumber')
			print result
                	value=result.split()
			if (result.find('nmm:fnumber')!=-1)  and (value[1]=='707') :
				print "property is set"
                	else:
				print "property is NOT set"
				Results[uri]='nmm:fnumber'

			overallRes.append(Results)
                for Result_dict in overallRes:
                        for k in Result_dict:
                                self.assert_(not k,'Writeback failed for following files for property %s\n\n' % (str(overallRes)) )


	def test_wb_11(self):
		"""
		Test if the value for flash property as flash-off is written to the file
		with sparql update with writeback.

                1. Delete the value of the property of the file.
                2. Insert a new value.
                3. Verify the value is written to the file.
                4. Run the instruction in for loop to test for all the files
                present in a particular directory.
		"""

		"""create list to catch unupdated file """
		overallRes = []

		for i in range(len(file_list)) :
			"""browse through each file """
			uri = 'file://'+file_list[i]
			print uri
			Results = {}

                	insert = """
				DELETE {?file nmm:flash ?val }
				WHERE { ?file nmm:flash ?val ;nie:url <%s> }
				INSERT {?file nmm:flash 'nmm:flash-off' }
				WHERE { ?file nie:url <%s> }
                		""" %(uri, uri)
			print insert
                	self.sparql_update (insert)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nmm:flash')
			print result
                	value=result.split()
			if (result.find('nmm:flash')!=-1)  and (value[1]=='nmm:flash-off') :
				print "property is set"
                	else:
				print "property is NOT set"
				Results[uri]='nmm:flash'

			overallRes.append(Results)
                for Result_dict in overallRes:
                        for k in Result_dict:
                                self.assert_(not k,'Writeback failed for following files for property %s\n\n' % (str(overallRes)) )

	def test_wb_12(self):
		"""
		Test if the value for flash property as flash-on is written to the file
		with sparql update with writeback.

                1. Delete the value of the property of the file.
                2. Insert a new value.
                3. Verify the value is written to the file.
                4. Run the instruction in for loop to test for all the files
                present in a particular directory.
		"""

		"""create list to catch unupdated file """
		overallRes = []

		for i in range(len(file_list)) :
			"""browse through each file """
			uri = 'file://'+file_list[i]
			print uri
			Results = {}

                	insert = """
				DELETE {?file nmm:flash ?val }
				WHERE { ?file nmm:flash ?val ;nie:url <%s> }
				INSERT {?file nmm:flash 'nmm:flash-on' }
				WHERE { ?file nie:url <%s> }
                		""" %(uri, uri)
			print insert
                	self.sparql_update (insert)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nmm:flash')
			print result
                	value=result.split()
			if (result.find('nmm:flash')!=-1)  and (value[1]=='nmm:flash-on') :
				print "property is set"
                	else:
				print "property is NOT set"
				Results[uri]='nmm:flash'

			overallRes.append(Results)
                for Result_dict in overallRes:
                        for k in Result_dict:
                                self.assert_(not k,'Writeback failed for following files for property %s\n\n' % (str(overallRes)) )


	def test_wb_13(self):
		"""
		Test if the value for focalLength property is written to the file
		with sparql update with writeback.

                1. Delete the value of the property of the file.
                2. Insert a new value.
                3. Verify the value is written to the file.
                4. Run the instruction in for loop to test for all the files
                present in a particular directory.
		"""

		"""create list to catch unupdated file """
		overallRes = []

		for i in range(len(file_list)) :
			"""browse through each file """
			uri = 'file://'+file_list[i]
			print uri
			Results = {}

                	insert = """
				DELETE {?file nmm:focalLength ?val }
				WHERE { ?file nmm:focalLength ?val ;nie:url <%s> }
				INSERT {?file nmm:focalLength '44' }
				WHERE { ?file nie:url <%s> }
                		""" %(uri, uri)
			print insert
                	self.sparql_update (insert)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nmm:focalLength')
			print result
                	value=result.split()
			if (result.find('nmm:focalLength')!=-1)  and (value[1]=='44') :
				print "property is set"
                	else:
				print "property is NOT set"
				Results[uri]='nmm:focalLength'

			overallRes.append(Results)
                for Result_dict in overallRes:
                        for k in Result_dict:
                                self.assert_(not k,'Writeback failed for following files for property %s\n\n' % (str(overallRes)) )

	def test_wb_14(self):
		"""
		Test if the value for isoSpeed property is written to the file
		with sparql update with writeback.

                1. Delete the value of the property of the file.
                2. Insert a new value.
                3. Verify the value is written to the file.
                4. Run the instruction in for loop to test for all the files
                present in a particular directory.
		"""

		"""create list to catch unupdated file """
		overallRes = []

		for i in range(len(file_list)) :
			"""browse through each file """
			uri = 'file://'+file_list[i]
			print uri
			Results = {}

                	insert = """
				DELETE {?file nmm:isoSpeed ?val }
				WHERE { ?file nmm:isoSpeed ?val ;nie:url <%s> }
				INSERT {?file nmm:isoSpeed '44' }
				WHERE { ?file nie:url <%s> }
                		""" %(uri, uri)
			print insert
                	self.sparql_update (insert)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nmm:isoSpeed')
			print result
                	value=result.split()
			if (result.find('nmm:isoSpeed')!=-1)  and (value[1]=='44') :
				print "property is set"
                	else:
				print "property is NOT set"
				Results[uri]='nmm:isoSpeed'

			overallRes.append(Results)
                for Result_dict in overallRes:
                        for k in Result_dict:
                                self.assert_(not k,'Writeback failed for following files for property %s\n\n' % (str(overallRes)) )

	def test_wb_15(self):
		"""
		Test if the value for meteringMode property is written to the file
		with sparql update with writeback.

                1. Delete the value of the property of the file.
                2. Insert a new value.
                3. Verify the value is written to the file.
                4. Run the instruction in for loop to test for all the files
                present in a particular directory.
		"""

		"""create list to catch unupdated file """
		overallRes = []

		for i in range(len(file_list)) :
			"""browse through each file """
			uri = 'file://'+file_list[i]
			print uri
			Results = {}

                	insert = """
				DELETE {?file nmm:meteringMode ?val }
				WHERE { ?file nmm:meteringMode ?val ;nie:url <%s> }
				INSERT {?file nmm:meteringMode 'nmm:metering-mode-multispot' }
				WHERE { ?file nie:url <%s> }
                		""" %(uri, uri)
			print insert
                	self.sparql_update (insert)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nmm:meteringMode')
			print result
                	value=result.split()
			if (result.find('nmm:meteringMode')!=-1)  and (value[1]=='nmm:metering-mode-multispot') :
				print "property is set"
                	else:
				print "property is NOT set"
				Results[uri]='nmm:meteringMode'

			overallRes.append(Results)
                for Result_dict in overallRes:
                        for k in Result_dict:
                                self.assert_(not k,'Writeback failed for following files for property %s\n\n' % (str(overallRes)) )

	def test_wb_16(self):
		"""
		Test if the value for whiteBalance property is written to the file
		with sparql update with writeback.

                1. Delete the value of the property of the file.
                2. Insert a new value.
                3. Verify the value is written to the file.
                4. Run the instruction in for loop to test for all the files
                present in a particular directory.
		"""

		"""create list to catch unupdated file """
		overallRes = []

		for i in range(len(file_list)) :
			"""browse through each file """
			uri = 'file://'+file_list[i]
			print uri
			Results = {}

                	insert = """
				DELETE {?file nmm:whiteBalance ?val }
				WHERE { ?file nmm:whiteBalance ?val ;nie:url <%s> }
				INSERT {?file nmm:whiteBalance 'nmm:white-balance-auto' }
				WHERE { ?file nie:url <%s> }
                		""" %(uri, uri)
			print insert
                	self.sparql_update (insert)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nmm:whiteBalance')
			print result
                	value=result.split()
			if (result.find('nmm:whiteBalance')!=-1)  and (value[1]=='nmm:white-balance-auto') :
				print "property is set"
                	else:
				print "property is NOT set"
				Results[uri]='nmm:whiteBalance'

			overallRes.append(Results)
                for Result_dict in overallRes:
                        for k in Result_dict:
                                self.assert_(not k,'Writeback failed for following files for property %s\n\n' % (str(overallRes)) )

	def test_wb_17(self):
		"""
		Test if the value for location property is written to the file
		with sparql update with writeback.

                1. Delete the value of the property of the file.
                2. Insert a new value.
                3. Verify the value is written to the file.
                4. Run the instruction in for loop to test for all the files
                present in a particular directory.
		"""

		"""create list to catch unupdated file """
		overallRes = []

		for i in range(len(file_list)) :
			"""browse through each file """
			uri = 'file://'+file_list[i]
			print uri
			Results = {}

                	insert = """
				DELETE {?file mlo:location ?val }
				WHERE { ?file mlo:location ?val ;nie:url <%s> }
				INSERT {?file mlo:location [ a mlo:GeoPoint ;mlo:country "clevland";mlo:city 'test_city';mlo:address 'doorno-32'] }
				WHERE { ?file nie:url <%s> }
                		""" %(uri, uri)
			print insert
                	self.sparql_update (insert)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep mlo:*')
			print result
                	value=result.split()
			if (result.find('mlo:address')!=-1)  and (value[value.index('mlo:address') + 1] == '"doorno-32"') and (result.find('mlo:city')!=-1)  and (value[value.index('mlo:city') + 1] == '"test_city"') and (result.find('mlo:country')!=-1)  and ('"clevland"' in value[value.index('mlo:country') + 1]) :
				print "property is set"
                	else:
				print "property is NOT set"
				Results[uri]='mlo:location'

			overallRes.append(Results)
                for Result_dict in overallRes:
                        for k in Result_dict:
                                self.assert_(not k,'Writeback failed for following files for property %s\n\n' % (str(overallRes)) )


if __name__ == "__main__":
        unittest.main()
