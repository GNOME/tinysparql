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

class TrackerCameraApplicationTests (CommonTrackerApplicationTest):

    def test_01_camera_picture (self):
        """
        Camera simulation:

        1. Create resource in the store for the new file
        2. Write the file
        3. Wait for miner-fs to index it
        4. Ensure no duplicates are found
        """

        fileurn = "tracker://test_camera_picture_01/" + str(random.randint (0,100))
        origin_filepath = os.path.join (self.get_data_dir (), self.get_test_image ())
        dest_filepath = os.path.join (self.get_dest_dir (), self.get_test_image ())
        dest_fileuri = "file://" + dest_filepath

        # Insert new resource in the store, including nie:mimeType and nie:url
        insert = """
        INSERT { <%s> a nie:InformationElement,
                        nie:DataObject,
                        nfo:Image,
                        nfo:Media,
                        nfo:Visual,
                        nmm:Photo
        }

        DELETE { <%s> nie:mimeType ?_1 }
        WHERE { <%s> nie:mimeType ?_1 }

        INSERT { <%s> a            rdfs:Resource ;
                      nie:mimeType \"image/jpeg\"
        }

        DELETE { <%s> nie:url ?_2 }
        WHERE { <%s> nie:url ?_2 }

        INSERT { <%s> a       rdfs:Resource ;
                      nie:url \"%s\"
        }
        """ % (fileurn, fileurn, fileurn, fileurn, fileurn, fileurn, fileurn, dest_fileuri)
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


    def test_02_camera_picture_geolocation (self):
        """
        Camera simulation:

        1. Create resource in the store for the new file
        2. Set nlo:location
        2. Write the file
        3. Wait for miner-fs to index it
        4. Ensure no duplicates are found
        """

        fileurn = "tracker://test_camera_picture_02/" + str(random.randint (0,100))
        dest_filepath = os.path.join (self.get_dest_dir (), self.get_test_image ())
        dest_fileuri = "file://" + dest_filepath

        geolocationurn = "tracker://test_camera_picture_02_geolocation/" + str(random.randint (0,100))
        postaladdressurn = "tracker://test_camera_picture_02_postaladdress/" + str(random.randint (0,100))

        # Insert new resource in the store, including nie:mimeType and nie:url
        insert = """
        INSERT { <%s> a nie:InformationElement,
                        nie:DataObject,
                        nfo:Image,
                        nfo:Media,
                        nfo:Visual,
                        nmm:Photo
        }

        DELETE { <%s> nie:mimeType ?_1 }
        WHERE { <%s> nie:mimeType ?_1 }

        INSERT { <%s> a            rdfs:Resource ;
                      nie:mimeType \"image/jpeg\"
        }

        DELETE { <%s> nie:url ?_2 }
        WHERE { <%s> nie:url ?_2 }

        INSERT { <%s> a       rdfs:Resource ;
                      nie:url \"%s\"
        }
        """ % (fileurn, fileurn, fileurn, fileurn, fileurn, fileurn, fileurn, dest_fileuri)
        self.tracker.update (insert)
        self.assertEquals (self.get_urn_count_by_url (dest_fileuri), 1)

        # FIRST, open the file for writing, and just write some garbage, to simulate that
        # we already started recording the video...
        fdest = open (dest_filepath, 'wb')
        fdest.write ("some garbage written here")
        fdest.write ("to simulate we're recording something...")
        fdest.seek (0)

        # SECOND, set slo:location
        location_insert = """
        INSERT { <%s> a             nco:PostalAddress ;
                      nco:country  \"SPAIN\" ;
                      nco:locality \"Tres Cantos\"
        }

        INSERT { <%s> a                 slo:GeoLocation ;
                      slo:postalAddress <%s>
        }

        INSERT { <%s> a            rdfs:Resource ;
                      slo:location <%s>
        }
        """ % (postaladdressurn, geolocationurn, postaladdressurn, fileurn, geolocationurn)
        self.tracker.update (location_insert)

        #THIRD, start copying the image to the dest path
        original_file = os.path.join (self.get_data_dir (),self.get_test_image ())
        self.slowcopy_file_fd (original_file, fdest)
        fdest.close ()
        assert os.path.exists (dest_filepath)

        # FOURTH, ensure we have only 1 resource
        self.system.tracker_miner_fs_wait_for_idle (MINER_FS_IDLE_TIMEOUT)
        self.assertEquals (self.get_urn_count_by_url (dest_fileuri), 1)

        # Clean the new file so the test directory is as before
        print "Remove and wait"
        os.remove (dest_filepath)
        self.system.tracker_miner_fs_wait_for_idle (MINER_FS_IDLE_TIMEOUT)
        self.assertEquals (self.get_urn_count_by_url (dest_fileuri), 0)


    def test_03_camera_video (self):
        """
        Camera video recording simulation:

        1. Create resource in the store for the new file
        2. Write the file
        3. Wait for miner-fs to index it
        4. Ensure no duplicates are found
        """

        fileurn = "tracker://test_camera_video_01/" + str(random.randint (0,100))
        origin_filepath = os.path.join (self.get_data_dir (), self.get_test_video ())
        dest_filepath = os.path.join (self.get_dest_dir (), self.get_test_video ())
        dest_fileuri = "file://" + dest_filepath

        # Insert new resource in the store, including nie:mimeType and nie:url
        insert = """
        INSERT { <%s> a nie:InformationElement,
                        nie:DataObject,
                        nfo:Video,
                        nfo:Media,
                        nfo:Visual,
                        nmm:Video
        }

        DELETE { <%s> nie:mimeType ?_1 }
        WHERE { <%s> nie:mimeType ?_1 }

        INSERT { <%s> a            rdfs:Resource ;
                      nie:mimeType \"video/mp4\"
        }

        DELETE { <%s> nie:url ?_2 }
        WHERE { <%s> nie:url ?_2 }

        INSERT { <%s> a       rdfs:Resource ;
                      nie:url \"%s\"
        }
        """ % (fileurn, fileurn, fileurn, fileurn, fileurn, fileurn, fileurn, dest_fileuri)
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


    def test_04_camera_video_geolocation (self):
        """
        Camera simulation:

        1. Create resource in the store for the new file
        2. Set nlo:location
        2. Write the file
        3. Wait for miner-fs to index it
        4. Ensure no duplicates are found
        """

        fileurn = "tracker://test_camera_video_02/" + str(random.randint (0,100))
        origin_filepath = os.path.join (self.get_data_dir (), self.get_test_video ())
        dest_filepath = os.path.join (self.get_dest_dir (), self.get_test_video ())
        dest_fileuri = "file://" + dest_filepath

        geolocationurn = "tracker://test_camera_video_02_geolocation/" + str(random.randint (0,100))
        postaladdressurn = "tracker://test_camera_video_02_postaladdress/" + str(random.randint (0,100))

        # Insert new resource in the store, including nie:mimeType and nie:url
        insert = """
        INSERT { <%s> a nie:InformationElement,
                        nie:DataObject,
                        nfo:Video,
                        nfo:Media,
                        nfo:Visual,
                        nmm:Video
        }

        DELETE { <%s> nie:mimeType ?_1 }
        WHERE { <%s> nie:mimeType ?_1 }

        INSERT { <%s> a            rdfs:Resource ;
                      nie:mimeType \"video/mp4\"
        }

        DELETE { <%s> nie:url ?_2 }
        WHERE { <%s> nie:url ?_2 }

        INSERT { <%s> a       rdfs:Resource ;
                      nie:url \"%s\"
        }
        """ % (fileurn, fileurn, fileurn, fileurn, fileurn, fileurn, fileurn, dest_fileuri)
        self.tracker.update (insert)
        self.assertEquals (self.get_urn_count_by_url (dest_fileuri), 1)

        # FIRST, open the file for writing, and just write some garbage, to simulate that
        # we already started recording the video...
        fdest = open (dest_filepath, 'wb')
        fdest.write ("some garbage written here")
        fdest.write ("to simulate we're recording something...")
        fdest.seek (0)

        # SECOND, set slo:location
        location_insert = """
        INSERT { <%s> a             nco:PostalAddress ;
                      nco:country  \"SPAIN\" ;
                      nco:locality \"Tres Cantos\"
        }

        INSERT { <%s> a                 slo:GeoLocation ;
                      slo:postalAddress <%s>
        }

        INSERT { <%s> a            rdfs:Resource ;
                      slo:location <%s>
        }
        """ % (postaladdressurn, geolocationurn, postaladdressurn, fileurn, geolocationurn)
        self.tracker.update (location_insert)

        #THIRD, start copying the image to the dest path
        self.slowcopy_file_fd (origin_filepath, fdest)
        fdest.close ()
        assert os.path.exists (dest_filepath)

        # FOURTH, ensure we have only 1 resource
        self.system.tracker_miner_fs_wait_for_idle (MINER_FS_IDLE_TIMEOUT)
        self.assertEquals (self.get_urn_count_by_url (dest_fileuri), 1)

        # Clean the new file so the test directory is as before
        print "Remove and wait"
        os.remove (dest_filepath)
        self.system.tracker_miner_fs_wait_for_idle (MINER_FS_IDLE_TIMEOUT)
        self.assertEquals (self.get_urn_count_by_url (dest_fileuri), 0)

if __name__ == "__main__":
	ut.main()


