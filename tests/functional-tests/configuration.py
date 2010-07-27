#!/usr/bin/python
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

# This is the configuration file for tracker testcases.
# It has many common utility defined.

import sys,os,dbus
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

PLATFORM_REL_PATH = '/etc/issue'

PREFIX = sys.prefix
CWD = os.getcwd()

TEST_IMAGE = "test-image-1.jpg"
TEST_IMAGE_PNG = "test-image-2.png"
TEST_IMAGE_TIF = "test-image-3.tif"
TEST_MUSIC = "tracker-mp3-test.mp3"
TEST_VIDEO = "test-video.mp4"

"""directory paths """
TEST_DIR = PREFIX + '/share/tracker-tests/'
TEST_DATA_DIR = PREFIX + '/share/tracker-tests/data/'
TEST_DATA_MUSIC = PREFIX + '/share/tracker-tests/data/Music/'
TEST_DATA_IMAGES = PREFIX + '/share/tracker-tests/data/Images/'
TEST_DATA_VIDEO = PREFIX + '/share/tracker-tests/data/Video/'

VCS_TEST_DATA_DIR = CWD + '/data/'
VCS_TEST_DATA_MUSIC = CWD + '/data/Music/'
VCS_TEST_DATA_IMAGES = CWD + '/data/Images/'
VCS_TEST_DATA_VIDEO = CWD + '/data/Video/'

"""
dir_path = os.environ['HOME']
"""
MYDOCS = '/home/user/MyDocs/'
MYDOCS_MUSIC = '/home/user/MyDocs/.sounds/'
MYDOCS_IMAGES = '/home/user/MyDocs/.images/'
MYDOCS_VIDEOS = '/home/user/MyDocs/.videos/'
WB_TEST_DIR_DEVICE = '/home/user/MyDocs/tracker-wb-test'
WB_TEST_DIR_HOST = os.path.expanduser("~") + '/tracker-wb-test'

URL_PREFIX = 'file://'

"""processes """
TRACKER_WRITEBACK = PREFIX + '/lib/tracker/tracker-writeback'
TRACKER_EXTRACT = PREFIX + '/lib/tracker/tracker-extract'
TRACKER_MINER = 'tracker-miner-fs'
TRACKER_STORE = 'tracker-store'
TRACKER_WRITEBACK_DESKTOP = PREFIX + '/local/libexec/tracker-writeback'
TRACKER_EXTRACT_DESKTOP = PREFIX + '/local/libexec/tracker-extract'

"""platforms constants"""
MAEMO6_SBOX = 2
MAEMO6_HW = 3
DESKTOP = 4

def check_target():
    """
    check_target retrieves platform
    """
    sboxindicator = '/targets/links/scratchbox.config'

    try :
        fhandle = open (PLATFORM_REL_PATH,'r')
        fcontent = fhandle.read()
        release = fcontent.lower().replace(' ','')
        fhandle.close()

        if "maemo" in release:
            if os.path.exists(sboxindicator) and \
            os.path.isfile(os.readlink(sboxindicator)) :
                return MAEMO6_SBOX
            else:
                return MAEMO6_HW
        else:
            return DESKTOP

    except IOError:
        print 'cannot open', PLATFORM_REL_PATH

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


    def set_test_data(self, target):
        """
        Copy test data on default location watched by tracker
        """
        if (target == DESKTOP):

            if (os.path.exists(os.path.expanduser("~") + '/' + TEST_MUSIC) \
                and os.path.exists(os.path.expanduser("~") + '/' + TEST_VIDEO) \
                and os.path.exists(os.path.expanduser("~") + '/' + TEST_IMAGE)):
                return

        elif (target == MAEMO6_HW):

            if (os.path.exists( MYDOCS_MUSIC + '/' + TEST_MUSIC) \
                and os.path.exists( MYDOCS_VIDEOS + '/' + TEST_VIDEO) \
                and os.path.exists( MYDOCS_IMAGES + '/' + TEST_IMAGE)):
                return

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

        if (target == DESKTOP):
            shutil.copy2(VCS_TEST_DATA_MUSIC + TEST_MUSIC, os.path.expanduser("~") + '/' + TEST_MUSIC)
            self.loop.run()
            shutil.copy2(VCS_TEST_DATA_VIDEO + TEST_VIDEO, os.path.expanduser("~") + '/' + TEST_VIDEO)
            self.loop.run()
            shutil.copy2(VCS_TEST_DATA_IMAGES + TEST_IMAGE, os.path.expanduser("~") + '/' + TEST_IMAGE)
            self.loop.run()

        if (target == MAEMO6_HW):
            shutil.copy2( TEST_DATA_MUSIC + TEST_MUSIC, MYDOCS_MUSIC + '/' + TEST_MUSIC)
            self.loop.run()
            shutil.copy2( TEST_DATA_VIDEO + TEST_VIDEO, MYDOCS_VIDEOS + '/' + TEST_VIDEO)
            self.loop.run()
            shutil.copy2( TEST_DATA_IMAGES + TEST_IMAGE, MYDOCS_IMAGES + '/' + TEST_IMAGE)
            self.loop.run()

