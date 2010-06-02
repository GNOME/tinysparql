#!/usr/bin/env python

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


import sys, os, dbus
import unittest
import time
import random
import commands
import string
import configuration as cfg

TRACKER = 'org.freedesktop.Tracker1'
TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources'
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources"

IMAGES_DEFAULT = cfg.MYDOCS_IMAGES
MUSIC_DEFAULT  = cfg.MYDOCS_MUSIC
VIDEOS_DEFAULT = cfg.MYDOCS_VIDEOS

def files_list(dirName):
    fileList = []
    for file in os.listdir(dirName):
        dirfile = os.path.join(dirName, file)
        if dirName == IMAGES_DEFAULT :
            if os.path.isfile(dirfile):
                if file.split('.')[1]== 'jpg' :
                    fileList.append(dirfile)

        elif dirName == MUSIC_DEFAULT :
            if os.path.isfile(dirfile):
                if file.split('.')[1]== 'mp3' :
                    fileList.append(dirfile)

        elif dirName == VIDEOS_DEFAULT :
            if os.path.isfile(dirfile):
                if file.split('.')[1]== 'avi' :
                    fileList.append(dirfile)
        return fileList

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

    def check ( self , appcn ):
        return int(commands.getoutput('pidof %s | wc -w' % appcn))

class tracker_daemon(TestUpdate):

    def test_miner_01(self) :
        appcn = cfg.TRACKER_MINER
        result=self.check(appcn)
        self.assert_(result==1,"tracker miner is not running" )

    def test_store_02(self) :
        appcn = cfg.TRACKER_STORE
        result=self.check(appcn)
        self.assert_(result==1,"tracker store is not running" )

class default_content(TestUpdate):

    def test_images_01(self) :
        """
        1.Check the no.of files in default folder
        2.Make tracker search for images and check if files are listed from default folders and count them.
        3.Check if no.of files from default folder is equal to tracker search results
        """
        default_Images=files_list(IMAGES_DEFAULT)
        Images  = commands.getoutput('tracker-search -i -l 10000 | grep /home/user/MyDocs/.images/|wc -l' )
        self.assert_(len(default_Images)==int(Images) , "Files are not indexed from default folder")

    def test_music_02(self) :

        """
        1.Check the no.of files in default folder
        2.Make tracker search for songs and check if files are listed from default folders and count them.
        3.Check if no.of files from default folder is equal to tracker search results
        """
        default_music=files_list(MUSIC_DEFAULT)
        Music  = commands.getoutput('tracker-search -m -l 10000 | grep /home/user/MyDocs/.sounds/|wc -l' )
        self.assert_(len(default_music)==int(Music) , "Files are not indexed from default folder")

    def test_video_03(self) :

        """
        1.Check the no.of files in default folder
        2.Make tracker search for videos and check if files are listed from default folders and count them.
        3.Check if no.of files from default folder is equal to tracker search results
        """

        default_videos=files_list(VIDEOS_DEFAULT)
        Videos  = commands.getoutput('tracker-search -v -l 10000 | grep /home/user/MyDocs/.videos/|wc -l' )
        self.assert_(len(default_videos)==int(Videos) , "Files are not indexed from default folder")

def run_tests(testNames):
    errors = []
    if (testNames == None):
        if (cfg.check_target() == cfg.MAEMO6_HW):
            suite_default_content = unittest.TestLoader().loadTestsFromTestCase(default_content)
            result = unittest.TextTestRunner(verbosity=2).run(suite_default_content)
            errors += result.errors + result.failures

        suite_tracker_daemon = unittest.TestLoader().loadTestsFromTestCase(tracker_daemon)
        result = unittest.TextTestRunner(verbosity=2).run(suite_tracker_daemon)
        errors += result.errors + result.failures
    else:
        suite = unittest.TestLoader().loadTestsFromNames(testNames, __import__('bootup-tc'))
        result = unittest.TextTestRunner(verbosity=2).run(suite)
        errors += result.errors + result.failures
    return len(errors)

def parseArgs(argv=None):
    import getopt
    if argv is None:
        argv = sys.argv
    testNames = None
    try:
        options, args = getopt.getopt(argv[1:], 'h')
        if len(args) == 0:
            # createTests will load tests from self.module
            testNames = None
        elif len(args) > 0:
            testNames = args
        run_tests(testNames)
    except getopt.error, msg:
        pass

if __name__ == "__main__":
    parseArgs()

