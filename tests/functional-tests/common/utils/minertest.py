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

from common.utils import configuration as cfg
from common.utils.helpers import StoreHelper
from common.utils.system import TrackerSystemAbstraction

import shutil
import os
import unittest2 as ut

MINER_TMP_DIR = cfg.TEST_MONITORED_TMP_DIR

def get_test_path (filename):
    return os.path.join (MINER_TMP_DIR, filename)

def get_test_uri (filename):
    return "file://" + os.path.join (MINER_TMP_DIR, filename)

DEFAULT_TEXT = "Some stupid content, to have a test file"

class CommonTrackerMinerTest (ut.TestCase):

    @classmethod
    def _create_test_data_simple (self):
        #
        #     ~/test-monitored/
        #                     /file1.txt
        #                     /dir1/
        #                          /file2.txt
        #                          /dir2/
        #                               /file3.txt
        #
        #
        #     ~/test-no-monitored/
        #                        /file0.txt
        #
        
        for d in ["test-monitored",
                  "test-monitored/dir1",
                  "test-monitored/dir1/dir2",
                  "test-no-monitored"]:
            directory = os.path.join (MINER_TMP_DIR, d)
            if (os.path.exists (directory)):
                shutil.rmtree (directory)
            os.makedirs (directory)

        for tf in ["test-monitored/file1.txt",
                   "test-monitored/dir1/file2.txt",
                   "test-monitored/dir1/dir2/file3.txt",
                   "test-no-monitored/file0.txt"]:
            testfile = os.path.join (MINER_TMP_DIR, tf)
            if (os.path.exists (testfile)):
                os.remove (testfile)
            f = open (testfile, 'w')
            f.write (DEFAULT_TEXT)
            f.close ()

    @classmethod
    def _create_test_data_many_files (self):
        # Create 10000 text files, so extraction takes a long time.

        directory = os.path.join (MINER_TMP_DIR, 'slow-extraction-data')
        #if (os.path.exists (directory)):
        #    shutil.rmtree (directory)
        if not os.path.exists (directory):
            os.makedirs (directory)

        # Extraction of 10,000 text files takes about 10 seconds on my system;
        # this is long enough to be able to detect bugs in extraction.
        # A more robust solution would be to create a mock subclass of
        # TrackerMinerFS in C and make it block in the process_files() callback.
        for i in range(10000):
            testfile = os.path.join (directory, "test-%i.txt" % i)
            if not os.path.exists (testfile):
                #    os.remove (testfile)
                f = open (testfile, 'w')
                f.write (DEFAULT_TEXT)
                f.close ()


    def _get_text_documents (self):
        return self.store.query ("""
          SELECT ?url WHERE {
              ?u a nfo:TextDocument ;
                 nie:url ?url.
          }
          """)

    def _get_parent_urn (self, filepath):
        result = self.store.query ("""
          SELECT nfo:belongsToContainer(?u) WHERE {
              ?u a nfo:FileDataObject ;
                 nie:url \"%s\" .
          }
          """ % (get_test_uri (filepath)))
        self.assertEquals (len (result), 1)
        return result[0][0]

    def _get_file_urn (self, filepath):
        result = self.store.query ("""
          SELECT ?u WHERE {
              ?u a nfo:FileDataObject ;
                 nie:url \"%s\" .
          }
          """ % (get_test_uri (filepath)))
        self.assertEquals (len (result), 1)
        return result[0][0]
