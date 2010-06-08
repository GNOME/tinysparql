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
import configuration
import commands
import signal
import shutil
from dbus.mainloop.glib import DBusGMainLoop
import gobject

TRACKER = 'org.freedesktop.Tracker1'
TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources'
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources"

MINER = "org.freedesktop.Tracker1.Miner.Files"
MINER_OBJ = "/org/freedesktop/Tracker1/Miner/Files"
MINER_IFACE = "org.freedesktop.Tracker1.Miner"


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
TARGET_VIDEO = dir_path + "/" + configuration.TEST_VIDEO

""" run the tracker-writeback """
pid = int(commands.getoutput('pidof tracker-writeback | wc -w'))

if not pid:
	os.system(tracker_wb + ' -v 3 &')
	time.sleep(5)



class TDCopy():

    def miner_processing_cb (self,status,handle):
        print "GOT PROGRESS FROM MINER"

        if (status == "Processing Files") :
            print "Miner started"
        elif (status == "Idle" ):
            """if the string is "Idle" quit the loop """
            print "Miner Idle"
            self.loop.quit()
        else :
            print "No specific Signal"


    def set_test_data(self):

        bus = dbus.SessionBus()
        tracker = bus.get_object(TRACKER, TRACKER_OBJ)
        self.resources = dbus.Interface (tracker,
                        dbus_interface=RESOURCES_IFACE)

        miner_obj= bus.get_object(MINER,MINER_OBJ)
        self.miner=dbus.Interface (miner_obj,dbus_interface=MINER_IFACE)


        self.loop = gobject.MainLoop()
        self.dbus_loop = DBusGMainLoop(set_as_default=True)
        self.bus = dbus.SessionBus (self.dbus_loop)

	self.bus.add_signal_receiver (self.miner_processing_cb,
                                  signal_name="Progress",
                                  dbus_interface=MINER_IFACE,
                                  path=MINER_OBJ)


    def copy_file(self, src, dest):
	shutil.copy2( src, dest)
	self.loop.run()

    def create_file_list_in_dir(self) :
	"""
	1. create a test directory
	2. copy images to test directory
	3. list the files present in the test data directory
	"""

	commands.getoutput('mkdir ' + dir_path)

	self.set_test_data()

	self.copy_file (configuration.TEST_DATA_IMAGES + configuration.TEST_IMAGE, dir_path)
	self.copy_file (configuration.TEST_DATA_IMAGES + configuration.TEST_IMAGE_PNG, dir_path)
	self.copy_file (configuration.TEST_DATA_IMAGES + configuration.TEST_IMAGE_TIF, dir_path)
	#TODO: uncomment once video writeback is supported
	#self.copy_file (configuration.TEST_DATA_VIDEO + configuration.TEST_VIDEO, dir_path)

        fileList = []
        dirpathList = []
        for dirpath,dirNames,fileNames in os.walk(dir_path):
		dirslist = os.listdir(dirpath)
       	        for filename in dirslist:
               	        fileList.append(dirpath+'/'+filename)

        return fileList


""" prepare the test data and get the list of files present in the test data directory """
tdcpy = TDCopy()
file_list = tdcpy.create_file_list_in_dir()



class TestWriteback (unittest.TestCase):

        def setUp(self):
                bus = dbus.SessionBus()
                tracker = bus.get_object(TRACKER, TRACKER_OBJ)
                self.resources = dbus.Interface (tracker,
                                                 dbus_interface=RESOURCES_IFACE)

                self.loop = gobject.MainLoop()
		self.dbus_loop=DBusGMainLoop(set_as_default=True)
                self.bus = dbus.SessionBus (self.dbus_loop)

                self.bus.add_signal_receiver (self.writeback_started,
					      signal_name="Writeback",
					      dbus_interface=RESOURCES_IFACE,
					      path=TRACKER_OBJ)

                self.bus.add_signal_receiver (self.writeback_ends,
					      signal_name="Progress",
					      dbus_interface=MINER_IFACE,
					      path=MINER_OBJ)

        def writeback_started (self, subject) :
               print "Writeback is started"

	def writeback_ends (self, status, handle) :
	       if status == "Processing Files" :
		   print "Writeback in Process"
	       elif status == "Idle" :
                   print "Writeback is Done"
		   self.loop.quit()



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

			start  = time.time ()
                	self.sparql_update (insert)
			self.loop.run()
			elapse = time.time ()-start
			print "===== Writeback is completed in %s secs ======== " %elapse

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
			self.loop.run()

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
			self.loop.run()

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
			self.loop.run()

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
			self.loop.run()

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
			self.loop.run()

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
			self.loop.run()

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
			if (file_list[i] == TARGET_VIDEO) :
				print "video file detected"
				continue

			uri = 'file://'+file_list[i]
			print uri
			Results = {}

                	insert = """
				DELETE {?file nfo:device ?val }
				WHERE { ?file nfo:device ?val ;nie:url <%s> }
				INSERT {?file nfo:device 'test camera' }
				WHERE { ?file nie:url <%s> }
                		""" %(uri, uri)
			print insert
                	self.sparql_update (insert)
			self.loop.run()

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nfo:device')
			print result
                	value=result.split()
			if (result.find('nfo:device')!=-1)  and (value[1]=='"test') and (value[2]=='camera"'):
				print "property is set"
                	else:
				print "property is NOT set"
				Results[uri]='nfo:device'

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
			if (file_list[i] == TARGET_VIDEO) :
				print "video file detected"
				continue

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
			time.sleep (2)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nmm:exposureTime')
			print result
			if not (result):
				print "property is NOT set"
				continue

                	value=result.split()
			if (result.find('nmm:exposureTime')!=-1)  and (value[1]=='44') :
				print "property is set"
				Results[uri]='nmm:exposureTime'
                	else:
				print "property is NOT set"

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
			if (file_list[i] == TARGET_VIDEO) :
				print "video file detected"
				continue

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
			time.sleep (2)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nmm:fnumber')
			print result
			if not (result):
				print "property is NOT set"
				continue

                	value=result.split()
			if (result.find('nmm:fnumber')!=-1)  and (value[1]=='707') :
				print "property is set"
				Results[uri]='nmm:fnumber'
                	else:
				print "property is NOT set"

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
			if (file_list[i] == TARGET_VIDEO) :
				print "video file detected"
				continue

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
			time.sleep (2)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nmm:flash')
			print result
			if not (result):
				print "property is NOT set"
				continue

                	value=result.split()
			if (result.find('nmm:flash')!=-1)  and (value[1]=='nmm:flash-off') :
				print "property is set"
				Results[uri]='nmm:flash'
                	else:
				print "property is NOT set"

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
			if (file_list[i] == TARGET_VIDEO) :
				print "video file detected"
				continue

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
			time.sleep (2)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nmm:flash')
			print result
			if not (result):
				print "property is NOT set"
				continue

                	value=result.split()
			if (result.find('nmm:flash')!=-1)  and (value[1]=='nmm:flash-on') :
				print "property is set"
				Results[uri]='nmm:flash'
                	else:
				print "property is NOT set"

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
			if (file_list[i] == TARGET_VIDEO) :
				print "video file detected"
				continue

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
			time.sleep (2)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nmm:focalLength')
			print result
			if not (result):
				print "property is NOT set"
				continue

                	value=result.split()
			if (result.find('nmm:focalLength')!=-1)  and (value[1]=='44') :
				print "property is set"
				Results[uri]='nmm:focalLength'
                	else:
				print "property is NOT set"

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
			if (file_list[i] == TARGET_VIDEO) :
				print "video file detected"
				continue

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
			time.sleep (2)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nmm:isoSpeed')
			print result
			if not (result):
				print "property is NOT set"
				continue

                	value=result.split()
			if (result.find('nmm:isoSpeed')!=-1)  and (value[1]=='44') :
				print "property is set"
				Results[uri]='nmm:isoSpeed'
                	else:
				print "property is NOT set"

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
			if (file_list[i] == TARGET_VIDEO) :
				print "video file detected"
				continue

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
			time.sleep (2)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nmm:meteringMode')
			print result
			if not (result):
				print "property is NOT set"
				continue

                	value=result.split()
			if (result.find('nmm:meteringMode')!=-1)  and (value[1]=='nmm:metering-mode-multispot') :
				print "property is set"
				Results[uri]='nmm:meteringMode'
                	else:
				print "property is NOT set"

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
			if (file_list[i] == TARGET_VIDEO) :
				print "video file detected"
				continue

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
			time.sleep (2)

                	""" verify the inserted item """
			result=commands.getoutput(tracker_ext + ' -f' +' ' + uri +' | grep nmm:whiteBalance')
			print result
			if not (result):
				print "property is NOT set"
				continue

                	value=result.split()
			if (result.find('nmm:whiteBalance')!=-1)  and (value[1]=='nmm:white-balance-auto') :
				print "property is set"
				Results[uri]='nmm:whiteBalance'
                	else:
				print "property is NOT set"

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
			self.loop.run()

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
