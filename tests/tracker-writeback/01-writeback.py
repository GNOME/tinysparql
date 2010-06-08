#!/usr/bin/env python
#
# Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

import dbus
import unittest
import random
import os
import shutil
import re
import time
import commands

TRACKER = 'org.freedesktop.Tracker1'
TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources'
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources"

class TestInsertion (unittest.TestCase):

    def setUp (self):
        bus = dbus.SessionBus ()
        tracker = bus.get_object (TRACKER, TRACKER_OBJ)
        self.resources = dbus.Interface (tracker,
                                         dbus_interface=RESOURCES_IFACE);

    def test_simple_insertion (self):
	try:        
	        os.mkdir (os.getcwd() + "/tmp")
	except:
		print ""

        shutil.copy2 (os.getcwd() + "/data/test01.jpg",
                      os.getcwd() + "/tmp/test01.jpg")

        uri = "file://" + os.getcwd() + "/tmp/test01.jpg"
        
        insert = """INSERT { <%s> a nfo:Image, nmm:Photo, nfo:FileDataObject;
                      nie:isStoredAs <%s> ;
                      nie:url '%s' ;
                      nie:title 'test_title_1' ;
                      nco:creator [ a nco:Contact ;
                                    nco:fullname 'test_fullname_1' ] ;
                      nie:description 'test_description_1' ;
                      nie:keyword 'test_keyword_1' ;
                      nie:keyword 'test_keyword_2' ;
                      nie:keyword 'test_keyword_3' ;
                      nie:contentCreated '2001-10-26T21:32:52' ;
                      nfo:orientation nfo:orientation-top-mirror ;
                      nmm:meteringMode nmm:metering-mode-average ;
                      nmm:whiteBalance nmm:white-balance-auto ;
                      nmm:flash nmm:flash-on ;
                      nmm:focalLength '1' ;
                      nmm:exposureTime '1' ;
                      nmm:isoSpeed '1' ;
                      nmm:fnumber '1' ;
                      nfo:device 'Some Test Model' ;
                      nco:contributor [ a nco:Contact ;
                                        nco:fullname 'test_fullname_2' ] ;
                      nie:copyright 'test_copyright_1'
               }""" % (uri, uri, uri)


        self.resources.SparqlUpdate (insert)

	time.sleep (3)

	ret = os.system ("exiftool " + os.getcwd() + "/tmp/test01.jpg | grep test_title_1")
        self.assertEqual (ret, 0)

	ret = os.system ("exiftool " + os.getcwd() + "/tmp/test01.jpg | grep test_fullname_1")
        self.assertEqual (ret, 0)

	ret = os.system ("exiftool " + os.getcwd() + "/tmp/test01.jpg | grep test_description_1")
        self.assertEqual (ret, 0)

	ret = os.system ("exiftool " + os.getcwd() + "/tmp/test01.jpg | grep test_keyword_1")
        self.assertEqual (ret, 0)

	ret = os.system ("exiftool " + os.getcwd() + "/tmp/test01.jpg | grep test_keyword_2")
        self.assertEqual (ret, 0)

	ret = os.system ("exiftool " + os.getcwd() + "/tmp/test01.jpg | grep test_keyword_3")
        self.assertEqual (ret, 0)

if __name__ == '__main__':
    unittest.main()
