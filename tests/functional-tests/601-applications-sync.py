#!/usr/bin/python
#
# Copyright (C) 2011, Nokia Corporation <ivan.frade@nokia.com>
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

"""
Tests trying to simulate the behaviour of applications working with tracker
"""

import sys,os,dbus
import unittest
import time
import random
import string
import datetime
import shutil
import fcntl

from common.utils import configuration as cfg
import unittest2 as ut
from common.utils.applicationstest import CommonTrackerApplicationTest as CommonTrackerApplicationTest

MINER_FS_IDLE_TIMEOUT = 5

class TrackerSyncApplicationTests (CommonTrackerApplicationTest):

    def test_sync_audio_01 (self):
        """
        Sync simulation (after fix for NB#219946):

        1. Create resource in the store for the new file, using blank nodes
        2. Write the file
        3. Wait for miner-fs to index it
        4. Ensure no duplicates are found
        """

        origin_filepath = os.path.join (self.get_data_dir (), self.get_test_music ())
        dest_filepath = os.path.join (self.get_dest_dir (), self.get_test_music ())
        dest_fileuri = "file://" + dest_filepath

        print "Synchronizing audio file in '%s'..." % (dest_filepath)

        # Insert new resource in the store
        insert = """
        DELETE { ?file a rdfs:Resource }
        WHERE  { ?file nie:url '%s'}

        INSERT { _:x a                       nie:DataObject,
                                             nmm:MusicPiece,
                                             nfo:Media,
                                             nfo:Audio,
                                             nie:InformationElement ;
                     nie:url                 '%s' ;
                     nmm:musicAlbum          <urn:album:SinCos> ;
                     nfo:duration            '15' ;
                     nmm:performer           <urn:artist:AbBaby> ;
                     nmm:trackNumber         '13' ;
                     nfo:averageAudioBitrate '32000' ;
                     nfo:genre               'Pop' ;
                     nfo:isContentEncrypted  'false' ;
                     nie:title               'Simply Juvenile'
        }

        INSERT { <urn:album:SinCos> a              nmm:MusicAlbum;
                                    nmm:albumTitle 'SinCos'
        }

        INSERT { <urn:artist:AbBaby> a              nmm:Artist;
                                     nmm:artistName 'AbBaby'
        }
        """ % (dest_fileuri, dest_fileuri)
        self.tracker.update (insert)
        self.assertEquals (self.get_urn_count_by_url (dest_fileuri), 1)

        # Copy the image to the dest path
        self.slowcopy_file (origin_filepath, dest_filepath)
        assert os.path.exists (dest_filepath)
        self.system.tracker_miner_fs_wait_for_idle (MINER_FS_IDLE_TIMEOUT)
        self.assertEquals (self.get_urn_count_by_url (dest_fileuri), 1)

        # Clean the new file so the test directory is as before
        print "Remove and wait"
        os.remove (dest_filepath)
        self.system.tracker_miner_fs_wait_for_idle (MINER_FS_IDLE_TIMEOUT)
        self.assertEquals (self.get_urn_count_by_url (dest_fileuri), 0)

if __name__ == "__main__":
	ut.main()


