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
import string

TRACKER = "org.freedesktop.Tracker1"
TRACKER_OBJ = "/org/freedesktop/Tracker1/Resources"
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources"
MINER="org.freedesktop.Tracker1.Miner.Files"
MINER_OBJ="/org/freedesktop/Tracker1/Miner/Files"
MINER_IFACE="org.freedesktop.Tracker1.Miner"


target = configuration.check_target()

if target == configuration.MAEMO6_HW:
	"""target is device """
        dir_path = configuration.MYDOCS
        dir_path_parent = '/home/user'
	src = configuration.TEST_DATA_IMAGES + 'test-image-1.jpg'

elif target == configuration.DESKTOP:
        dir_path = os.path.expanduser("~")
        dir_path_parent = os.path.expanduser("~") + "/" + "tmp"
	if (not (os.path.exists(dir_path_parent) and os.path.isdir(dir_path_parent))):
	    os.mkdir (dir_path_parent)
	src = configuration.VCS_TEST_DATA_IMAGES + 'test-image-1.jpg'

print dir_path

""" copy the test data to proper location. """
def copy_file():

        dest = dir_path
        print 'Copying '+src+' to '+dest
        commands.getoutput('cp '+src+ ' '+dest)

copy_file()

class TestVirtualFiles (unittest.TestCase):
        def setUp(self):
                bus = dbus.SessionBus()
                tracker = bus.get_object(TRACKER, TRACKER_OBJ)
                self.resources = dbus.Interface (tracker, dbus_interface=RESOURCES_IFACE)
		miner_obj= bus.get_object(MINER,MINER_OBJ)
		self.miner=dbus.Interface (miner_obj,dbus_interface=MINER_IFACE)

        def sparql_update(self,query):
                return self.resources.SparqlUpdate(query)
        def query(self,query):
                return self.resources.SparqlQuery(query)
	def ignore(self,uri):
		return self.miner.IgnoreNextUpdate(uri)


class virtual_files(TestVirtualFiles):

	def test_Virttual_01(self):
                """
                Test if the update is ignored until the creation of the file is completed.
                1. Move the file to some other location.
                2. Create resource in tracker , by making instance of nie:DataObject.
                3. IgnoreNextUpdate on the files.
                4. Copy the original file to the present directory.
                5. Query for the title of the file.
                """

                test_file = 'test-image-1.jpg'
                file= dir_path + '/' + test_file
                uri='file://' + file
		print uri

                commands.getoutput('mv  ' + file + ' ' + dir_path_parent)

                Insert = """
		INSERT { _:x a nfo:Image, nie:DataObject ;
                nie:url <%s> ;
                nie:title 'title_test'. }""" %(uri)
		print Insert

                self.sparql_update(Insert)
		time.sleep(10)

                self.miner.IgnoreNextUpdate([uri])

                commands.getoutput('cp ' + dir_path_parent + '/'+ test_file + ' ' + dir_path)

                QUERY = """
                SELECT ?t WHERE { ?file a nfo:FileDataObject ;
                nie:title ?t ;
                nie:url <%s> .}
                """ %(uri)
		print QUERY

                result=self.query(QUERY)
		print result

                self.assert_(result[0][0].find('title_test')!=-1 , "File is not ignored")


	def test_Virtual_02(self):

		"""
		1) Insert in tracker a "virtual" file (can be a text file) tagged as Favourite
		2) Start writing the file (with some sleep to make the process long)
		3) Close the file, wait for tracker to discover it
		4) Check the metadata of the file AND that the tag (favourite) is there
                """

                test_file = 'testfilename.txt'
                file= dir_path + '/' + test_file
                url='file://' + file
		print url

		insert="""
		INSERT { _:x a nfo:Image, nie:DataObject ; \
		nie:url <%s> ; \
		nie:title 'title_test';
		nao:hasTag [a nao:Tag ; nao:prefLabel "Favorite"] ;
		nie:plainTextContent 'This is script to test virtual file support'.
		}
		""" %url
		self.sparql_update(insert)

		time.sleep(3)

	        QUERY="""
                SELECT ?label ?content WHERE { ?file a nie:DataObject ;nao:hasTag[a nao:Tag ;nao:prefLabel ?label]; nie:url <%s> ;
		nie:plainTextContent ?content.
                }
                """ %url

		result=self.query(QUERY)

		self.assert_(result[0][0].find('Favorite')!=-1 and result[1][1].find("This is script to test virtual file support")!=-1, "File is not monitored by tracker")



if __name__ == "__main__":

        unittest.main()
	if (os.path.exists(dir_path_parent) and os.path.isdir(dir_path_parent)):
	    os.rmdir (dir_path_parent)
