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
from common.utils.system import TrackerSystemAbstraction
import unittest2 as ut

from gi.repository import GLib

import shutil
import os
import warnings
from itertools import chain

MINER_TMP_DIR = cfg.TEST_MONITORED_TMP_DIR

def path (filename):
    return os.path.join (MINER_TMP_DIR, filename)

def uri (filename):
    return "file://" + os.path.join (MINER_TMP_DIR, filename)


DEFAULT_TEXT = "Some stupid content, to have a test file"

index_dirs = [os.path.join (MINER_TMP_DIR, "test-monitored")]
CONF_OPTIONS = {
    cfg.DCONF_MINER_SCHEMA: {
        'index-recursive-directories': GLib.Variant.new_strv(index_dirs),
        'index-single-directories': GLib.Variant.new_strv([]),
        'index-optical-discs': GLib.Variant.new_boolean(False),
        'index-removable-devices': GLib.Variant.new_boolean(False),
        'throttle': GLib.Variant.new_int32(5),
    }
}


class CommonTrackerMinerTest (ut.TestCase):

    def prepare_directories (self):
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

        monitored_files = [
            'test-monitored/file1.txt',
            'test-monitored/dir1/file2.txt',
            'test-monitored/dir1/dir2/file3.txt'
        ]

        unmonitored_files = [
            'test-no-monitored/file0.txt'
        ]

        def ensure_dir_exists(dirname):
            if not os.path.exists(dirname):
                os.makedirs(dirname)

        for tf in chain(monitored_files, unmonitored_files):
            testfile = path(tf)
            ensure_dir_exists(os.path.dirname(testfile))
            with open (testfile, 'w') as f:
                f.write (DEFAULT_TEXT)

        for tf in monitored_files:
            self.tracker.await_resource_inserted(
                'nfo:TextDocument', url=uri(tf))

    def setUp (self):
        for d in ['test-monitored', 'test-no-monitored']:
            dirname = path(d)
            if os.path.exists (dirname):
                shutil.rmtree(dirname)
            os.makedirs(dirname)

        self.system = TrackerSystemAbstraction ()

        self.system.tracker_miner_fs_testing_start (CONF_OPTIONS)
        self.tracker = self.system.store

        try:
            self.prepare_directories ()
            self.tracker.reset_graph_updates_tracking ()
        except Exception as e:
            self.tearDown ()
            raise

    def tearDown (self):
        self.system.tracker_miner_fs_testing_stop ()

    def assertResourceExists (self, urn):
        if self.tracker.ask ("ASK { <%s> a rdfs:Resource }" % urn) == False:
            self.fail ("Resource <%s> does not exist" % urn)

    def assertResourceMissing (self, urn):
        if self.tracker.ask ("ASK { <%s> a rdfs:Resource }" % urn) == True:
            self.fail ("Resource <%s> should not exist" % urn)
