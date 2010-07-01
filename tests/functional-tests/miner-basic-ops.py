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


import sys,os,dbus
import unittest
import time
import random
import commands
import configuration
from dbus.mainloop.glib import DBusGMainLoop
import gobject
import shutil

TRACKER = 'org.freedesktop.Tracker1'
TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources/Classes'
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources.Class"

MINER="org.freedesktop.Tracker1.Miner.Files"
MINER_OBJ="/org/freedesktop/Tracker1/Miner/Files"
MINER_IFACE="org.freedesktop.Tracker1.Miner"

TEST_IMAGE = "test-image-1.jpg"
TEST_MUSIC = "tracker-mp3-test.mp3"
TEST_VIDEO = "test-video.mp4"

"""create two test directories in miner monitored path  """
target = configuration.check_target()

if target == configuration.MAEMO6_HW:
    """target is device """
    TEST_DIR_1 = configuration.MYDOCS + "tracker_test_op_1"
    TEST_DIR_2 = configuration.MYDOCS + "tracker_test_op_2"
    """create a test directory in miner un-monitored path  """
    #TEST_DIR_3 = configuration.TEST_DIR + "tracker_test_op_3"
    TEST_DIR_3 = configuration.MYDOCS + "core-dumps/" + "tracker_test_op_3"
    SRC_IMAGE_DIR = configuration.TEST_DATA_IMAGES
    SRC_MUSIC_DIR = configuration.TEST_DATA_MUSIC
    SRC_VIDEO_DIR = configuration.TEST_DATA_VIDEO
    MYDOCS_SUB =  configuration.MYDOCS + 's1/s2/s3/s4/s5/'

elif target == configuration.DESKTOP:
    """target is DESKTOP """
    TEST_DIR_1 = os.path.expanduser("~") + '/' + "tracker_test_op_1"
    TEST_DIR_2 = os.path.expanduser("~") + '/' + "tracker_test_op_2"
    TEST_DIR_3 = os.path.expanduser("~") + '/' + "core-dumps/" + "tracker_test_op_3"
    SRC_IMAGE_DIR = configuration.VCS_TEST_DATA_IMAGES
    SRC_MUSIC_DIR = configuration.VCS_TEST_DATA_MUSIC
    SRC_VIDEO_DIR = configuration.VCS_TEST_DATA_VIDEO
    MYDOCS_SUB =  os.path.expanduser("~") + 's1/s2/s3/s4/s5/'

commands.getoutput('mkdir ' + TEST_DIR_1)
commands.getoutput('mkdir ' + TEST_DIR_2)
commands.getoutput('mkdir -p ' + TEST_DIR_3)

class TestUpdate (unittest.TestCase):

    def setUp(self):
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

    def wait_for_fileop (self, cmd, src, dst=''):
        if (cmd == "rm"):
            os.remove(src)
        elif (cmd == "cp"):
            shutil.copy2(src, dst)
        else:
            shutil.move(src,dst)
        self.loop.run()

""" copy operation and tracker-miner response test cases """
class copy(TestUpdate):

    def test_copy_01 (self):

        """Copy an image file from unmonitored directory to monitored directory
        and verify if data base is updated accordingly"""


        file_path = TEST_DIR_1 + '/test-image-copy-01.jpg'

        """ 1. Copy an image file from unmonitored directory to monitored directory """
        self.wait_for_fileop('cp', SRC_IMAGE_DIR + TEST_IMAGE, file_path)

        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path + ' | wc -l')
        print result
        self.assert_(result == '1' , 'copied file is not shown as indexed')

        self.wait_for_fileop('rm', file_path)

    def test_copy_02 (self):

        """Copy a music  file from unmonitored directory to monitored directory
        and verify if data base is updated accordingly"""


        file_path = TEST_DIR_1 + '/test-music-copy-01.mp3'

        """ 1. Copy file from unmonitored directory to monitored directory """
        self.wait_for_fileop('cp', SRC_MUSIC_DIR + TEST_MUSIC, file_path)


        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path + ' | wc -l')
        print result
        self.assert_(result == '1' , 'copied file is not shown as indexed')

        self.wait_for_fileop('rm', file_path)

    def test_copy_03 (self):

        """Copy a video  file from unmonitored directory to monitored directory
        and verify if data base is updated accordingly"""


        file_path = TEST_DIR_1 + '/test-video-copy-01.mp4'

        """ 1. Copy file from unmonitored directory to monitored directory """
        self.wait_for_fileop('cp', SRC_VIDEO_DIR + TEST_VIDEO, file_path)


        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path + ' | wc -l')
        print result
        self.assert_(result == '1' , 'copied file is not shown as indexed')

        self.wait_for_fileop('rm', file_path)

    def test_copy_04 (self):

        """Copy an image file from monitored directory to unmonitored directory
        and verify if data base is updated accordingly"""


        file_path_1 = TEST_DIR_1 + '/test-image-copy-01.jpg'
        file_path_2 = TEST_DIR_3 + '/test-image-copy-01.jpg'

        """ 1. Copy an image file to monitored directory """
        self.wait_for_fileop('cp', SRC_IMAGE_DIR + TEST_IMAGE, file_path_1)


        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path_1 + ' | wc -l')
        print result
        if result == '1':
            print "file copied and indexed"
        else :
            self.fail("file not indexed")


        """ 3. Copy an image file to unmonitored directory """
        commands.getoutput('cp '+ file_path_1 + ' ' + file_path_2)
        time.sleep(2)

        """ 4. verify if miner indexed these files.  """
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path_2 + ' | wc -l')
        self.assert_(result == '0' , 'copied file is shown as indexed')
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path_1 + ' | wc -l')
        self.assert_(result == '1' , 'source file is not shown as indexed')

        self.wait_for_fileop('rm', file_path_1)
        os.remove(file_path_2)

    def test_copy_05 (self):

        """Copy a music file from monitored directory to unmonitored directory
        and verify if data base is updated accordingly"""


        file_path_1 = TEST_DIR_1 + '/test-music-copy-01.mp3'
        file_path_2 = TEST_DIR_3 + '/test-music-copy-01.mp3'

        """ 1. Copy file to monitored directory """
        self.wait_for_fileop('cp', SRC_MUSIC_DIR + TEST_MUSIC, file_path_1)


        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_1 + ' | wc -l')
        print result
        if result == '1':
            print "file copied and indexed"
        else :
            self.fail("file not indexed")


        """ 3. Copy file to unmonitored directory """
        commands.getoutput('cp '+ file_path_1 + ' ' + file_path_2)
        time.sleep(2)

        """ 4. verify if miner indexed these files.  """
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_2 + ' | wc -l')
        self.assert_(result == '0' , 'copied file is shown as indexed')
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_1 + ' | wc -l')
        self.assert_(result == '1' , 'source file is not shown as indexed')

        self.wait_for_fileop('rm', file_path_1)
        os.remove(file_path_2)

    def test_copy_06 (self):

        """Copy a video file from monitored directory to unmonitored directory
        and verify if data base is updated accordingly"""


        file_path_1 = TEST_DIR_1 + '/test-video-copy-01.mp4'
        file_path_2 = TEST_DIR_3 + '/test-video-copy-01.mp4'

        """ 1. Copy file to monitored directory """
        self.wait_for_fileop('cp', SRC_VIDEO_DIR + TEST_VIDEO, file_path_1)


        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_1 + ' | wc -l')
        print result
        if result == '1':
            print "file copied and indexed"
        else :
            self.fail("file not indexed")


        """ 3. Copy file to unmonitored directory """
        commands.getoutput('cp '+ file_path_1 + ' ' + file_path_2)
        time.sleep(2)

        """ 4. verify if miner indexed these files.  """
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_2 + ' | wc -l')
        self.assert_(result == '0' , 'copied file is shown as indexed')
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_1 + ' | wc -l')
        self.assert_(result == '1' , 'source file is not shown as indexed')

        self.wait_for_fileop('rm', file_path_1)
        os.remove(file_path_2)


    def test_copy_07 (self):

        """Copy an image file from monitored directory to another monitored directory
        and verify if data base is updated accordingly"""


        file_path_1 = TEST_DIR_1 + '/test-image-copy-01.jpg'
        file_path_2 = TEST_DIR_2 + '/test-image-copy-01.jpg'

        """ 1. Copy an image file to monitored directory """
        self.wait_for_fileop('cp', SRC_IMAGE_DIR + TEST_IMAGE, file_path_1)


        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path_1 + ' | wc -l')
        print result
        if result == '1':
            print "file copied and indexed"
        else :
            self.fail("file not indexed")


        """ 3. Copy an image file to another monitored directory """
        self.wait_for_fileop('cp', file_path_1, file_path_2)


        """ 4. verify if miner indexed these files.  """
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path_2 + ' | wc -l')
        self.assert_(result == '1' , 'copied file is not shown as indexed')
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path_1 + ' | wc -l')
        self.assert_(result == '1' , 'source file is not shown as indexed')

        self.wait_for_fileop('rm', file_path_1)
        self.wait_for_fileop('rm', file_path_2)

    def test_copy_08 (self):

        """Copy a music file from monitored directory to another monitored directory
        and verify if data base is updated accordingly"""


        file_path_1 = TEST_DIR_1 + '/test-music-copy-01.mp3'
        file_path_2 = TEST_DIR_2 + '/test-music-copy-01.mp3'

        """ 1. Copy file to monitored directory """
        self.wait_for_fileop('cp', SRC_MUSIC_DIR + TEST_MUSIC, file_path_1)


        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_1 + ' | wc -l')
        print result
        if result == '1':
            print "file copied and indexed"
        else :
            self.fail("file not indexed")


        """ 3. Copy file to another monitored directory """
        self.wait_for_fileop('cp', file_path_1, file_path_2)


        """ 4. verify if miner indexed both of these file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_2 + ' | wc -l')
        self.assert_(result == '1' , 'copied file is not shown as indexed')
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_1 + ' | wc -l')
        self.assert_(result == '1' , 'source file is not shown as indexed')

        self.wait_for_fileop('rm', file_path_1)
        self.wait_for_fileop('rm', file_path_2)


    def test_copy_09 (self):

        """Copy a video file from monitored directory to another monitored directory
        and verify if data base is updated accordingly"""


        file_path_1 = TEST_DIR_1 + '/test-video-copy-01.mp4'
        file_path_2 = TEST_DIR_2 + '/test-video-copy-01.mp4'

        """ 1. Copy file to monitored directory """
        self.wait_for_fileop('cp', SRC_VIDEO_DIR + TEST_VIDEO, file_path_1)


        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_1 + ' | wc -l')
        print result
        if result == '1':
            print "file copied and indexed"
        else :
            self.fail("file not indexed")


        """ 3. Copy file to another monitored directory """
        self.wait_for_fileop('cp', file_path_1, file_path_2)


        """ 4. verify if miner indexed both of these file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_2 + ' | wc -l')
        self.assert_(result == '1' , 'copied file is not shown as indexed')
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_1 + ' | wc -l')
        self.assert_(result == '1' , 'source file is not shown as indexed')

        self.wait_for_fileop('rm', file_path_1)
        self.wait_for_fileop('rm', file_path_2)

""" move operation and tracker-miner response test cases """
class move(TestUpdate):


    def test_move_01 (self):

        """move an image file from unmonitored directory to monitored directory
        and verify if data base is updated accordingly"""

        file_path_1 = TEST_DIR_3 + '/test-image-move-01.jpg'
        file_path_2 = TEST_DIR_1 + '/test-image-move-01.jpg'

        """ 1. Copy an image file to an unmonitored directory """
        commands.getoutput('cp '+ SRC_IMAGE_DIR + TEST_IMAGE + ' ' + file_path_1)

        """ 1. move an image file from unmonitored directory to monitored directory """
        self.wait_for_fileop('mv', file_path_1, file_path_2)


        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path_2 + ' | wc -l')
        print result
        self.assert_(result == '1' , 'moved file is not shown as indexed')

        self.wait_for_fileop('rm', file_path_2)


    def test_move_02 (self):

        """move a music  file from unmonitored directory to monitored directory
        and verify if data base is updated accordingly"""


        file_path_1 = TEST_DIR_3 + '/test-music-move-01.mp3'
        file_path_2 = TEST_DIR_1 + '/test-music-move-01.mp3'

        """ 1. Copy file to an unmonitored directory """
        commands.getoutput('cp '+ SRC_MUSIC_DIR + TEST_MUSIC + ' ' + file_path_1)


        """ 1. move file from unmonitored directory to monitored directory """
        self.wait_for_fileop('mv', file_path_1, file_path_2)


        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_2 + ' | wc -l')
        print result
        self.assert_(result == '1' , 'moved file is not shown as indexed')

        self.wait_for_fileop('rm', file_path_2)


    def test_move_03 (self):

        """move a video  file from unmonitored directory to monitored directory
        and verify if data base is updated accordingly"""


        file_path_1 = TEST_DIR_3 + '/test-video-move-01.mp4'
        file_path_2 = TEST_DIR_1 + '/test-video-move-01.mp4'

        """ 1. Copy file to an unmonitored directory """
        commands.getoutput('cp '+ SRC_VIDEO_DIR + TEST_VIDEO + ' ' + file_path_1)

        """ 1. move file from unmonitored directory to monitored directory """
        self.wait_for_fileop('mv', file_path_1, file_path_2)


        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_2 + ' | wc -l')
        print result
        self.assert_(result == '1' , 'moved file is not shown as indexed')

        self.wait_for_fileop('rm', file_path_2)


    def test_move_04 (self):

        """move an image file from monitored directory to unmonitored directory
        and verify if data base is updated accordingly"""


        file_path_1 = TEST_DIR_1 + '/test-image-move-01.jpg'
        file_path_2 = TEST_DIR_3 + '/test-image-move-01.jpg'

        """ 1. copy an image file to monitored directory """
        self.wait_for_fileop('cp', SRC_IMAGE_DIR + TEST_IMAGE, file_path_1)


        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path_1 + ' | wc -l')
        print result
        if result == '1':
            print "file moved and indexed"
        else :
            self.fail("file not indexed")


        """ 3. move an image file to unmonitored directory """
        self.wait_for_fileop('mv', file_path_1, file_path_2)


        """ 4. verify if miner indexed these files.  """
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path_2 + ' | wc -l')
        self.assert_(result == '0' , 'moveed file is shown as indexed')
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path_1 + ' | wc -l')
        self.assert_(result == '0' , 'source file is shown as indexed')

        os.remove(file_path_2)

    def test_move_05 (self):

        """move a music file from monitored directory to unmonitored directory
        and verify if data base is updated accordingly"""


        file_path_1 = TEST_DIR_1 + '/test-music-move-01.mp3'
        file_path_2 = TEST_DIR_3 + '/test-music-move-01.mp3'

        """ 1. copy file to monitored directory """
        self.wait_for_fileop('cp', SRC_MUSIC_DIR + TEST_MUSIC, file_path_1)


        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_1 + ' | wc -l')
        print result
        if result == '1':
            print "file moved and indexed"
        else :
            self.fail("file not indexed")


        """ 3. move file to unmonitored directory """
        self.wait_for_fileop('mv', file_path_1, file_path_2)


        """ 4. verify if miner indexed these files.  """
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_2 + ' | wc -l')
        self.assert_(result == '0' , 'moveed file is shown as indexed')
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_1 + ' | wc -l')
        self.assert_(result == '0' , 'source file is shown as indexed')

        os.remove(file_path_2)

    def test_move_06 (self):

        """move a video file from monitored directory to unmonitored directory
        and verify if data base is updated accordingly"""


        file_path_1 = TEST_DIR_1 + '/test-video-move-01.mp4'
        file_path_2 = TEST_DIR_3 + '/test-video-move-01.mp4'

        """ 1. copy file to monitored directory """
        self.wait_for_fileop('cp', SRC_VIDEO_DIR + TEST_VIDEO, file_path_1)


        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_1 + ' | wc -l')
        print result
        if result == '1':
            print "file moved and indexed"
        else :
            self.fail("file not indexed")


        """ 3. move file to unmonitored directory """
        self.wait_for_fileop('mv', file_path_1, file_path_2)


        """ 4. verify if miner indexed these files.  """
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_2 + ' | wc -l')
        self.assert_(result == '0' , 'moveed file is shown as indexed')
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_1 + ' | wc -l')
        self.assert_(result == '0' , 'source file is shown as indexed')

        os.remove(file_path_2)

    def test_move_07 (self):

        """move an image file from monitored directory to another monitored directory
        and verify if data base is updated accordingly"""


        file_path_1 = TEST_DIR_1 + '/test-image-move-01.jpg'
        file_path_2 = TEST_DIR_2 + '/test-image-move-01.jpg'

        """ 1. Copy an image file to monitored directory """
        self.wait_for_fileop('cp', SRC_IMAGE_DIR + TEST_IMAGE, file_path_1)


        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path_1 + ' | wc -l')
        print result
        if result == '1':
            print "file copied and indexed"
        else :
            self.fail("file not indexed")


        """ 3. move an image file to another monitored directory """
        self.wait_for_fileop('mv', file_path_1, file_path_2)


        """ 4. verify if miner indexed these files.  """
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path_2 + ' | wc -l')
        self.assert_(result == '1' , 'moveed file is not shown as indexed')
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path_1 + ' | wc -l')
        self.assert_(result == '0' , 'source file is shown as indexed')

        self.wait_for_fileop('rm', file_path_2)

    def test_move_08 (self):

        """move a music file from monitored directory to another monitored directory
        and verify if data base is updated accordingly"""


        file_path_1 = TEST_DIR_1 + '/test-music-move-01.mp3'
        file_path_2 = TEST_DIR_2 + '/test-music-move-01.mp3'

        """ 1. Copy file to monitored directory """
        self.wait_for_fileop('cp', SRC_MUSIC_DIR + TEST_MUSIC, file_path_1)


        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_1 + ' | wc -l')
        print result
        if result == '1':
            print "file copied and indexed"
        else :
            self.fail("file not indexed")


        """ 3. move file to another monitored directory """
        self.wait_for_fileop('mv', file_path_1, file_path_2)


        """ 4. verify if miner indexed both of these file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_2 + ' | wc -l')
        self.assert_(result == '1' , 'moved file is not shown as indexed')
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_1 + ' | wc -l')
        self.assert_(result == '0' , 'source file is shown as indexed')

        self.wait_for_fileop('rm', file_path_2)

    def test_move_09 (self):

        """move a video file from monitored directory to another monitored directory
        and verify if data base is updated accordingly"""


        file_path_1 = TEST_DIR_1 + '/test-video-move-01.mp4'
        file_path_2 = TEST_DIR_2 + '/test-video-move-01.mp4'

        """ 1. Copy file to monitored directory """
        self.wait_for_fileop('cp', SRC_VIDEO_DIR + TEST_VIDEO, file_path_1)


        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_1 + ' | wc -l')
        print result
        if result == '1':
            print "file copied and indexed"
        else :
            self.fail("file not indexed")


        """ 3. move file to another monitored directory """
        self.wait_for_fileop('mv', file_path_1, file_path_2)


        """ 4. verify if miner indexed both of these file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_2 + ' | wc -l')
        self.assert_(result == '1' , 'moved file is not shown as indexed')
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_1 + ' | wc -l')
        self.assert_(result == '0' , 'source file is shown as indexed')

        self.wait_for_fileop('rm', file_path_2)

""" delete operation and tracker-miner response test cases """
class delete(TestUpdate):


    def test_delete_01 (self):

        """Delete an image file and verify if data base is updated accordingly"""


        file_path = TEST_DIR_1 + '/test-image-delete-01.jpg'

        """ 1. Copy test image file from test data dir to a monitored dir """
        self.wait_for_fileop('cp', SRC_IMAGE_DIR + TEST_IMAGE, file_path)

        """verify the image is indexed """
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path + ' | wc -l')
        print result
        if result == '1':
            print "file copied and indexed"
        else :
            self.fail("file not indexed")

        """ 2. Delete the image file from monitored dir """
        self.wait_for_fileop('rm', file_path)

        """verify the deleted image is not indexed """
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path + ' | wc -l')
        print result
        self.assert_(result == '0' , 'deleted file is shown as indexed')

    def test_delete_02 (self):

        """Delete an audio file from monitored directory and verify if data base is updated accordingly"""

        file_path = TEST_DIR_1 + '/test-music-delete-01.mp3'

        """ 1. Copy test music file from test data dir to a monitored dir """
        self.wait_for_fileop('cp', SRC_MUSIC_DIR + TEST_MUSIC, file_path)

        """verify the file is indexed """
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path + ' | wc -l')
        print result
        if result == '1':
            print "file copied and indexed"
        else :
            self.fail("file not indexed")

        """ 2. Delete the file """
        self.wait_for_fileop('rm', file_path)

        """verify the deleted image is not indexed """
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path + ' | wc -l')
        print result
        self.assert_(result == '0' , 'deleted file is shown as indexed')

    def test_delete_03 (self):

        """Delete a video file from monitored directory and verify if data base is updated accordingly"""

        file_path = TEST_DIR_1 + '/test-video-delete-01.mp4'

        """ 1. Copy test music file from test data dir to a monitored dir """
        self.wait_for_fileop('cp', SRC_VIDEO_DIR + TEST_VIDEO, file_path)

        """verify the file is indexed """
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path + ' | wc -l')
        print result
        if result == '1':
            print "file copied and indexed"
        else :
            self.fail("file not indexed")

        """ 2. Delete the file """
        self.wait_for_fileop('rm', file_path)

        """verify the deleted file is not indexed """
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path + ' | wc -l')
        print result
        self.assert_(result == '0' , 'deleted file is shown as indexed')

class subfolders(TestUpdate) :

    def test_subfolders_01(self):

        """
        1.Create multilevel directories.
        2.Copy an image to the directory.
        3.Check if tracker-search is listing the copied file.
        4.Remove the file from directory.
        5.Check if tracker-search is not listing the file.
        """
        commands.getoutput('mkdir -p '+ MYDOCS_SUB)
        print MYDOCS_SUB,SRC_IMAGE_DIR,TEST_IMAGE
        commands.getoutput('cp ' + SRC_IMAGE_DIR + TEST_IMAGE + ' ' + MYDOCS_SUB)
        self.loop.run()

        result = commands.getoutput('tracker-search -i -l 5000 | grep '+MYDOCS_SUB+TEST_IMAGE+' |wc -l')
        self.assert_(int(result)==1 , "File is not indexed")

        commands.getoutput ('rm '+MYDOCS_SUB+TEST_IMAGE)
        self.loop.run()

        result1 = commands.getoutput('tracker-search -i -l 5000 | grep '+MYDOCS_SUB+TEST_IMAGE +'|wc -l')
        self.assert_(int(result1)==0 , "File is still listed in tracker search")


    def test_subfolders_02(self):

        """
        1.Create multilevel directories.
        2.Copy an song to the directory.
        3.Check if tracker-search is listing the copied file.
        4.Remove the file from directory.
        5.Check if tracker-search is not listing the file.
        """
        commands.getoutput('mkdir -p '+ MYDOCS_SUB)
        commands.getoutput('cp ' + SRC_MUSIC_DIR + TEST_MUSIC + ' ' + MYDOCS_SUB)
        self.loop.run()

        result = commands.getoutput('tracker-search -m -l 5000 | grep '+ MYDOCS_SUB+TEST_MUSIC +'| wc -l ')
        self.assert_(int(result)==1 , "File is not indexed")

        commands.getoutput ('rm '+MYDOCS_SUB+TEST_MUSIC)
        self.loop.run()

        result1 = commands.getoutput('tracker-search -m -l 5000 | grep '+MYDOCS_SUB+TEST_MUSIC +'|wc -l')
        self.assert_(int(result1)==0 , "File is still listed in tracker search")





if __name__ == "__main__":
    unittest.main()
