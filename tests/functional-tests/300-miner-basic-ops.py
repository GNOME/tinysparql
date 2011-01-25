#!/usr/bin/python

# Copyright (C) 2010, Nokia (ivan.frade@nokia.com)
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

#
# TODO:
#     These tests are for files... we need to write them for folders!
#
"""
Monitor a test directory and copy/move/remove/update files and folders there.
Check the basic data of the files is updated accordingly in tracker.
"""
import os
import shutil
import time

import unittest2 as ut
from common.utils.minertest import CommonTrackerMinerTest, BASEDIR, uri, path

class MinerCrawlTest (CommonTrackerMinerTest):
    """
    Test cases to check if miner is able to monitor files that are created, deleted or moved
    """
    def __get_text_documents (self):
        return self.tracker.query ("""
          SELECT ?url WHERE {
              ?u a nfo:TextDocument ;
                 nie:url ?url.
          }
          """)

    def __get_parent_urn (self, filepath):
        result = self.tracker.query ("""
          SELECT nfo:belongsToContainer(?u) WHERE {
              ?u a nfo:FileDataObject ;
                 nie:url \"%s\" .
          }
          """ % (uri (filepath)))
        self.assertEquals (len (result), 1)
        return result[0][0]

    def __get_file_urn (self, filepath):
        result = self.tracker.query ("""
          SELECT ?u WHERE {
              ?u a nfo:FileDataObject ;
                 nie:url \"%s\" .
          }
          """ % (uri (filepath)))
        self.assertEquals (len (result), 1)
        return result[0][0]

    def tearDown (self):
        # Give it a 2 seconds chance
        result = self.__get_text_documents ()
        if (len (result) != 3):
            time.sleep (2)
        else:
            return

        result = self.__get_text_documents ()
        if (len (result) != 3):
            print "WARNING: Previous test has modified the test files and didn't restore the origina state."

    """
    Boot the miner with the correct configuration and check everything is fine
    """
    def test_01_initial_crawling (self):
        """
        The precreated files and folders should be there
        """
        # Maybe the information hasn't been committed yet
        time.sleep (1)
        result = self.__get_text_documents ()
        self.assertEquals (len (result), 3)
        unpacked_result = [ r[0] for r in result]
        self.assertIn ( uri ("test-monitored/file1.txt"), unpacked_result)
        self.assertIn ( uri ("test-monitored/dir1/file2.txt"), unpacked_result)
        self.assertIn ( uri ("test-monitored/dir1/dir2/file3.txt"), unpacked_result)

        # We don't check (yet) folders, because Applications module is injecting results


## class copy(TestUpdate):
## FIXME all tests in one class because the miner-fs restarting takes some time (~5 sec)
##       Maybe we can move the miner-fs initialization to setUpModule and then move these
##       tests to different classes

    def test_02_copy_from_unmonitored_to_monitored (self):
        """
        Copy an file from unmonitored directory to monitored directory
        and verify if data base is updated accordingly
        """
        source = os.path.join (BASEDIR, "test-no-monitored", "file0.txt")
        dest = os.path.join (BASEDIR, "test-monitored", "file0.txt")
        shutil.copyfile (source, dest)
        self.system.tracker_miner_fs_wait_for_idle ()

        # verify if miner indexed this file.
        result = self.__get_text_documents ()
        self.assertEquals (len (result), 4)
        unpacked_result = [ r[0] for r in result]
        self.assertIn ( uri ("test-monitored/file1.txt"), unpacked_result)
        self.assertIn ( uri ("test-monitored/dir1/file2.txt"), unpacked_result)
        self.assertIn ( uri ("test-monitored/dir1/dir2/file3.txt"), unpacked_result)
        self.assertIn ( uri ("test-monitored/file0.txt"), unpacked_result)

        # Clean the new file so the test directory is as before
        print "Remove and wait"
        os.remove (dest)
        self.system.tracker_miner_fs_wait_for_idle ()

    def test_03_copy_from_monitored_to_unmonitored (self):
        """
        Copy an file from a monitored location to an unmonitored location
        Nothing should change
        """

        # Copy from monitored to unmonitored
        source = os.path.join (BASEDIR, "test-monitored", "file1.txt")
        dest = os.path.join (BASEDIR, "test-no-monitored", "file1.txt")
        shutil.copyfile (source, dest)

        time.sleep (1)
        # Nothing changed
        result = self.__get_text_documents ()
        self.assertEquals (len (result), 3, "Results:" + str(result))
        unpacked_result = [ r[0] for r in result]
        self.assertIn ( uri ("test-monitored/file1.txt"), unpacked_result)
        self.assertIn ( uri ("test-monitored/dir1/file2.txt"), unpacked_result)
        self.assertIn ( uri ("test-monitored/dir1/dir2/file3.txt"), unpacked_result)

        # Clean the file
        os.remove (dest)

    def test_04_copy_from_monitored_to_monitored (self):
        """
        Copy a file between monitored directories
        """
        source = os.path.join (BASEDIR, "test-monitored", "file1.txt")
        dest = os.path.join (BASEDIR, "test-monitored", "dir1", "dir2", "file-test04.txt")
        shutil.copyfile (source, dest)
        self.system.tracker_miner_fs_wait_for_idle ()

        result = self.__get_text_documents ()
        self.assertEquals (len (result), 4)
        unpacked_result = [ r[0] for r in result]
        self.assertIn ( uri ("test-monitored/file1.txt"), unpacked_result)
        self.assertIn ( uri ("test-monitored/dir1/file2.txt"), unpacked_result)
        self.assertIn ( uri ("test-monitored/dir1/dir2/file3.txt"), unpacked_result)
        self.assertIn ( uri ("test-monitored/dir1/dir2/file-test04.txt"), unpacked_result)

        # Clean the file
        os.remove (dest)
        self.system.tracker_miner_fs_wait_for_idle ()
        self.assertEquals (3, self.tracker.count_instances ("nfo:TextDocument"))


    def test_05_move_from_unmonitored_to_monitored (self):
        """
        Move a file from unmonitored to monitored directory
        """
        source = os.path.join (BASEDIR, "test-no-monitored", "file0.txt")
        dest = os.path.join (BASEDIR, "test-monitored", "dir1", "file-test05.txt")
        shutil.move (source, dest)
        self.system.tracker_miner_fs_wait_for_idle ()

        result = self.__get_text_documents ()
        self.assertEquals (len (result), 4)
        unpacked_result = [ r[0] for r in result]
        self.assertIn ( uri ("test-monitored/file1.txt"), unpacked_result)
        self.assertIn ( uri ("test-monitored/dir1/file2.txt"), unpacked_result)
        self.assertIn ( uri ("test-monitored/dir1/dir2/file3.txt"), unpacked_result)
        self.assertIn ( uri ("test-monitored/dir1/file-test05.txt"), unpacked_result)

        # Clean the file
        os.remove (dest)
        self.system.tracker_miner_fs_wait_for_idle ()
        self.assertEquals (3, self.tracker.count_instances ("nfo:TextDocument"))

## """ move operation and tracker-miner response test cases """
## class move(TestUpdate):


    def test_06_move_from_monitored_to_unmonitored (self):
        """
        Move a file from monitored to unmonitored directory
        """
        source = os.path.join (BASEDIR, "test-monitored", "dir1", "file2.txt")
        dest = os.path.join (BASEDIR, "test-no-monitored", "file2.txt")
        shutil.move (source, dest)
        self.system.tracker_miner_fs_wait_for_idle ()

        result = self.__get_text_documents ()
        self.assertEquals (len (result), 2)
        unpacked_result = [ r[0] for r in result]
        self.assertIn ( uri ("test-monitored/file1.txt"), unpacked_result)
        self.assertIn ( uri ("test-monitored/dir1/dir2/file3.txt"), unpacked_result)

        # Restore the file
        shutil.move (dest, source)
        self.system.tracker_miner_fs_wait_for_idle ()
        self.assertEquals (3, self.tracker.count_instances ("nfo:TextDocument"))


    def test_07_move_from_monitored_to_monitored (self):
        """
        Move a file between monitored directories
        """
        source = os.path.join (BASEDIR, "test-monitored", "dir1", "file2.txt")
        dest = os.path.join (BASEDIR, "test-monitored", "file2.txt")

        source_dir_urn = self.__get_file_urn (os.path.join (BASEDIR, "test-monitored", "dir1"))
        parent_before = self.__get_parent_urn (source)
        self.assertEquals (source_dir_urn, parent_before)

        shutil.move (source, dest)
        self.system.tracker_miner_fs_wait_for_idle ()

        # Checking fix for NB#214413: After a move operation, nfo:belongsToContainer
        # should be changed to the new one
        dest_dir_urn = self.__get_file_urn (os.path.join (BASEDIR, "test-monitored"))
        parent_after = self.__get_parent_urn (dest)
        self.assertNotEquals (parent_before, parent_after)
        self.assertEquals (dest_dir_urn, parent_after)

        result = self.__get_text_documents ()
        self.assertEquals (len (result), 3)
        unpacked_result = [ r[0] for r in result]
        self.assertIn ( uri ("test-monitored/file1.txt"), unpacked_result)
        self.assertIn ( uri ("test-monitored/file2.txt"), unpacked_result)
        self.assertIn ( uri ("test-monitored/dir1/dir2/file3.txt"), unpacked_result)

        # Restore the file
        shutil.move (dest, source)
        self.system.tracker_miner_fs_wait_for_idle ()

        result = self.__get_text_documents ()
        self.assertEquals (len (result), 3)
        unpacked_result = [ r[0] for r in result]
        self.assertIn ( uri ("test-monitored/dir1/file2.txt"), unpacked_result)


    def test_08_deletion_single_file (self):
        """
        Delete one of the files
        """
        victim = os.path.join (BASEDIR, "test-monitored", "dir1", "file2.txt")
        os.remove (victim)
        self.system.tracker_miner_fs_wait_for_idle ()

        result = self.__get_text_documents ()
        self.assertEquals (len (result), 2)
        unpacked_result = [ r[0] for r in result]
        self.assertIn ( uri ("test-monitored/file1.txt"), unpacked_result)
        self.assertIn ( uri ("test-monitored/dir1/dir2/file3.txt"), unpacked_result)

        # Restore the file
        f = open (victim, "w")
        f.write ("Don't panic, everything is fine")
        f.close ()
        self.system.tracker_miner_fs_wait_for_idle ()

    def test_09_deletion_directory (self):
        """
        Delete a directory
        """
        victim = os.path.join (BASEDIR, "test-monitored", "dir1")
        shutil.rmtree (victim)
        self.system.tracker_miner_fs_wait_for_idle ()

        result = self.__get_text_documents ()
        self.assertEquals (len (result), 1)
        unpacked_result = [ r[0] for r in result]
        self.assertIn ( uri ("test-monitored/file1.txt"), unpacked_result)

        # Restore the dirs
        #  Wait after each operation to be sure of the results
        os.makedirs (os.path.join (BASEDIR, "test-monitored", "dir1"))
        self.system.tracker_miner_fs_wait_for_idle ()
        os.makedirs (os.path.join (BASEDIR, "test-monitored", "dir1", "dir2"))
        self.system.tracker_miner_fs_wait_for_idle ()
        for f in ["test-monitored/dir1/file2.txt",
                  "test-monitored/dir1/dir2/file3.txt"]:
            filename = os.path.join (BASEDIR, f)
            writer = open (filename, "w")
            writer.write ("Don't panic, everything is fine")
            writer.close ()
            self.system.tracker_miner_fs_wait_for_idle ()

        # Wait a bit more... some time one idle is not enough
        self.system.tracker_miner_fs_wait_for_idle (3)

        # Check everything is fine
        result = self.__get_text_documents ()
        self.assertEquals (len (result), 3)

if __name__ == "__main__":
    print """
     Tests for Copy/move/delete operations of FILES between monitored/unmonitored locations.

     We need to do the same for DIRECTORIES!
    """
    ut.main()
