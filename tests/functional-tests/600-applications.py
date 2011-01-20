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

from common.utils import configuration as cfg
import unittest2 as ut
from common.utils.applicationstest import CommonTrackerApplicationTest as CommonTrackerApplicationTest, APPLICATIONS_TMP_DIR, path, uri, slowcopy

TEST_IMAGE = "test-image-1.jpg"
SRC_IMAGE_DIR = os.path.join (cfg.DATADIR,
                              "tracker-tests",
                              "data",
                              "Images")
SRC_IMAGE_PATH = os.path.join (SRC_IMAGE_DIR, TEST_IMAGE)
MINER_FS_IDLE_TIMEOUT = 5


class TrackerApplicationTests (CommonTrackerApplicationTest):

    def __get_urn_count_by_url (self, url):
        select = """
        SELECT ?u WHERE { ?u nie:url \"%s\" }
        """ % (url)
        return len (self.tracker.query (select))


    def test_camera_insert_01 (self):
        """
        Camera simulation:

        1. Create resource in the store for the new file
        2. Write the file
        3. Wait for miner-fs to index it
        4. Ensure no duplicates are found
        """

        fileurn = "tracker://test_camera_insert_01/" + str(random.randint (0,100))
        filepath = path (TEST_IMAGE)
        fileuri = uri (TEST_IMAGE)

        print "Storing new image in '%s'..." % (filepath)

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
        """ % (fileurn, fileurn, fileurn, fileurn, fileurn, fileurn, fileurn, fileuri)
        self.tracker.update (insert)
        self.assertEquals (self.__get_urn_count_by_url (fileuri), 1)

        # Copy the image to the dest path, simulating the camera writting
        slowcopy (SRC_IMAGE_PATH, filepath, 1024)
        assert os.path.exists (filepath)
        self.system.tracker_miner_fs_wait_for_idle (MINER_FS_IDLE_TIMEOUT)
        self.assertEquals (self.__get_urn_count_by_url (fileuri), 1)

        # Clean the new file so the test directory is as before
        print "Remove and wait"
        os.remove (filepath)
        self.system.tracker_miner_fs_wait_for_idle (MINER_FS_IDLE_TIMEOUT)
        self.assertEquals (self.__get_urn_count_by_url (fileuri), 0)


    def test_camera_insert_02 (self):
        """
        Camera simulation:

        1. Create resource in the store for the new file
        2. Set nlo:location
        2. Write the file
        3. Wait for miner-fs to index it
        4. Ensure no duplicates are found
        """

        fileurn = "tracker://test_camera_insert_02/" + str(random.randint (0,100))
        filepath = path (TEST_IMAGE)
        fileuri = uri (TEST_IMAGE)

        geolocationurn = "tracker://test_camera_insert_02_geolocation/" + str(random.randint (0,100))
        postaladdressurn = "tracker://test_camera_insert_02_postaladdress/" + str(random.randint (0,100))

        print "Storing new image in '%s'..." % (filepath)

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
        """ % (fileurn, fileurn, fileurn, fileurn, fileurn, fileurn, fileurn, fileuri)
        self.tracker.update (insert)
        self.assertEquals (self.__get_urn_count_by_url (fileuri), 1)


        # Copy the image to the dest path, simulating the camera writting
        slowcopy (SRC_IMAGE_PATH, filepath, 1024)
        assert os.path.exists (filepath)
        self.system.tracker_miner_fs_wait_for_idle (MINER_FS_IDLE_TIMEOUT)
        self.assertEquals (self.__get_urn_count_by_url (fileuri), 1)

        # Add slo:location to the new resource...
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
        self.system.tracker_miner_fs_wait_for_idle (MINER_FS_IDLE_TIMEOUT)
        self.assertEquals (self.__get_urn_count_by_url (fileuri), 1)

        # Clean the new file so the test directory is as before
        print "Remove and wait"
        os.remove (filepath)
        self.system.tracker_miner_fs_wait_for_idle (MINER_FS_IDLE_TIMEOUT)
        self.assertEquals (self.__get_urn_count_by_url (fileuri), 0)

if __name__ == "__main__":
	ut.main()


