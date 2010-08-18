#!/usr/bin/python
#-*- coding: utf-8 -*-

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
Monitor a directory, copy/move/remove/update text files and check that
the text contents are updated accordingly in the indexes.
"""
import os
import shutil
import locale

import unittest2 as ut
from common.utils.minertest import CommonTrackerMinerTest, BASEDIR, uri, path, DEFAULT_TEXT
from common.utils import configuration as cfg

class CommonMinerFTS (CommonTrackerMinerTest):
    """
    Superclass to share methods. Shouldn't be run by itself.
    """
    def setUp (self):
        self.testfile = "test-monitored/miner-fts-test.txt"
        if os.path.exists (path (self.testfile)):
            os.remove (path (self.testfile))
        # Shouldn't we wait here for the miner to idle? (it works without it)
            
    def tearDown (self):
        #if os.path.exists (path (self.testfile)):
        #    os.remove (path (self.testfile))
        pass

    def set_text (self, text):
        f = open (path (self.testfile), "w")
        f.write (text)
        f.close ()
        self.system.tracker_miner_fs_wait_for_idle ()

    def search_word (self, word):
        """
        Return list of URIs with the word in them
        """
        print word
        results = self.tracker.query ("""
                SELECT ?url WHERE {
                  ?u a nfo:TextDocument ;
                      nie:url ?url ;
                      fts:match '%s'.
                 }
                 """ % (word))
        return [r[0] for r in results]
   
    def basic_test (self, text, word):
        """
        Save the text on the testfile, search the word
        and assert the testfile is only result.

        Be careful with the default contents of the text files
        ( see common/utils/minertest.py DEFAULT_TEXT )
        """
        self.set_text (text)
        results = self.search_word (word)
        self.assertEquals (len (results), 1)
        self.assertIn ( uri (self.testfile), results)



class MinerFTSBasicTest (CommonMinerFTS):
    """
    Tests different contents in a single file
    """

    def test_01_single_word (self):
        TEXT = "automobile"
        self.basic_test (TEXT, TEXT)

    def test_02_multiple_words (self):
        TEXT = "automobile with unlimited power"
        self.set_text (TEXT)
        
        results = self.search_word ("automobile")
        self.assertEquals (len (results), 1)
        self.assertIn (uri (self.testfile), results)

        results = self.search_word ("unlimited")
        self.assertEquals (len (results), 1)
        self.assertIn (uri (self.testfile), results)


    def test_03_long_word (self):
        # TEXT is longer than the 20 characters specified in the fts configuration
        TEXT = "fsfsfsdfskfweeqrewqkmnbbvkdasdjefjewriqjfnc"
        self.set_text (TEXT)

        results = self.search_word (TEXT)
        self.assertEquals (len (results), 0)

    def test_04_non_existent_word (self):
        TEXT = "This a trick"
        self.set_text (TEXT)
        results = self.search_word ("trikc")
        self.assertEquals (len (results), 0)


    def test_05_word_in_multiple_files (self):
        # Safeguard, in the case we modify the DEFAULT_TEXT later...
        assert "content" in DEFAULT_TEXT

        self.set_text (DEFAULT_TEXT)
        results = self.search_word ("content")
        self.assertEquals (len (results), 4)
        self.assertIn ( uri (self.testfile), results)
        self.assertIn ( uri ("test-monitored/file1.txt"), results)
        self.assertIn ( uri ("test-monitored/dir1/file2.txt"), results)
        self.assertIn ( uri ("test-monitored/dir1/dir2/file3.txt"), results)

    def test_06_word_multiple_times_in_file (self):
        TEXT = "automobile is red. automobile is big. automobile is great!"
        self.basic_test (TEXT, "automobile")

    def test_07_sentence (self):
        TEXT = "plastic is fantastic"
        self.basic_test (TEXT, TEXT)

    def test_08_partial_sentence (self):
        TEXT = "plastic is fantastic"
        self.basic_test (TEXT, "is fantastic")

    def test_09_strange_word (self):
        # FIXME Not sure what are we testing here
        TEXT = "'summer.time'"
        self.basic_test (TEXT, "summer.time")

    # Skip the test 'search for .'

    def test_10_mixed_letters_and_numbers (self):
        TEXT = "abc123"
        self.basic_test (TEXT, "abc123")

    def test_11_ignore_numbers (self):
        TEXT = "palabra 123123"
        self.set_text (TEXT)
        results = self.search_word ("123123")
        self.assertEquals (len (results), 0)


class MinerFTSFileOperationsTest (CommonMinerFTS):
    """
    Move, update, delete the files and check the text indexes are updated accordingly.
    """

    def test_01_removal_of_file (self):
        """
        When removing the file, its text contents disappear from the index
        """
        TEXT = "automobile is red and big and whatnot"
        self.basic_test (TEXT, "automobile")

        os.remove ( path (self.testfile))
        self.system.tracker_miner_fs_wait_for_idle ()

        results = self.search_word ("automobile")
        self.assertEquals (len (results), 0)

    def test_02_empty_the_file (self):
        """
        Emptying the file, the indexed words are also removed
        """
        TEXT = "automobile is red and big and whatnot"
        self.basic_test (TEXT, "automobile")

        self.set_text ("")
        results = self.search_word ("automobile")
        self.assertEquals (len (results), 0)

    def test_03_update_the_file (self):
        """
        Changing the contents of the file, updates the index
        """
        TEXT = "automobile is red and big and whatnot"
        self.basic_test (TEXT, "automobile")

        self.set_text ("airplane is blue and small and wonderful")
        results = self.search_word ("automobile")
        self.assertEquals (len (results), 0)

        results = self.search_word ("airplane")
        self.assertEquals (len (results), 1)

    # Skip the test_text_13... feel, feet, fee in three diff files and search feet

    def __recreate_file (self, filename, content):
        if os.path.exists (filename):
            os.remove (filename)

        f = open (filename, "w")
        f.write (content)
        f.close ()
        

    def test_04_on_unmonitored_file (self):
        """
        Set text in an unmonitored file. There should be no results.
        """
        TEXT = "automobile is red"

        TEST_15_FILE = "test-no-monitored/fts-indexing-test-15.txt"
        self.__recreate_file (path (TEST_15_FILE), TEXT)

        results = self.search_word ("automobile")
        self.assertEquals (len (results), 0)

        os.remove (path (TEST_15_FILE))

    def test_05_move_file_unmonitored_monitored (self):
        """
        Move file from unmonitored location to monitored location and index should be updated
        """

        # Maybe the miner hasn't finished yet with the setUp deletion!
        self.system.tracker_miner_fs_wait_for_idle ()
        
        TEXT = "airplane is beautiful"
        TEST_16_SOURCE = "test-no-monitored/fts-indexing-text-16.txt"
        TEST_16_DEST = "test-monitored/fts-indexing-text-16.txt"
        
        self.__recreate_file (path (TEST_16_SOURCE), TEXT)

        results = self.search_word ("airplane")
        self.assertEquals (len (results), 0)

        shutil.copyfile ( path (TEST_16_SOURCE), path (TEST_16_DEST))
        self.system.tracker_miner_fs_wait_for_idle ()

        results = self.search_word ("airplane")
        self.assertEquals (len (results), 1)

        os.remove ( path (TEST_16_SOURCE))
        os.remove ( path (TEST_16_DEST))

    # skip test for a file in a hidden directory

class MinerFTSStopwordsTest (CommonMinerFTS):
    """
    Search for stopwords in a file 
    """

    def __get_some_stopwords (self):

        langcode, encoding = locale.getdefaultlocale ()
        if "_" in langcode:
            langcode = langcode.split ("_")[0]

        stopwordsfile = os.path.join (cfg.DATADIR, "tracker", "languages", "stopwords." + langcode)

        if not os.path.exists (stopwordsfile):
            self.skipTest ("No stopwords for the current locale ('%s' doesn't exist)" % (stopwordsfile))
            return []
        
        stopwords = []
        counter = 0
        for line in open (stopwordsfile, "r"):
            if len (line) > 4:
                stopwords.append (line[:-1])
                counter += 1

            if counter > 5:
                break
            
        return stopwords
    
    def test_01_stopwords (self):
        stopwords = self.__get_some_stopwords ()
        TEXT = " ".join (["this a completely normal text automobile"] + stopwords)
        
        self.set_text (TEXT)
        results = self.search_word ("automobile")
        self.assertEquals (len (results), 1)
        print stopwords
        for i in range (0, len (stopwords)):
            results = self.search_word (stopwords[i])
            self.assertEquals (len (results), 0)

    ## FIXME add all the special character tests!
    ##  http://git.gnome.org/browse/tracker/commit/?id=81c0d3bd754a6b20ac72323481767dc5b4a6217b
    

if __name__ == "__main__":
    ut.main ()
