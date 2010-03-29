#!/usr/bin/python
#
# Copyright (C) 2010, Nokia
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
# Define the location of the test data in testDataDir
# This also checks if the program is running in target or in host and return the column of pid

import os, sys

prefix = sys.prefix

"""directory paths """
TEST_DIR = prefix + '/share/tracker-tests/'
TEST_DATA_DIR = prefix + '/share/tracker-tests/data/'
TEST_DATA_MUSIC = prefix + '/share/tracker-tests/data/Music/'
TEST_DATA_IMAGES = prefix + '/share/tracker-tests/data/Images/'

"""
dir_path = os.environ['HOME']
"""
MYDOCS_MUSIC = '/home/user/MyDocs/.sounds/'
MYDOCS_IMAGES = '/home/user/MyDocs/.images/'
WB_TEST_DIR_DEVICE = '/home/user/MyDocs/tracker-wb-test'
WB_TEST_DIR_HOST = prefix + '/share/tracker-tests/data/tracker-wb-test'

URL_PREFIX = 'file://'

"""processes """
TRACKER_WRITEBACK = prefix + '/lib/tracker/tracker-writeback'
TRACKER_EXTRACT = prefix + '/lib/tracker/tracker-extract'

def dir_path():
        return testDataDir

'''
def check_target():
        on_target = 'OSSO_PRODUCT_NAME'
        try :
                if os.environ[on_target]:
                #if os.environ['DBUS_SESSION_BUS_ADDRESS'] == 'unix:path=/tmp/session_bus_socket':
                        awk_print = '1'
                        return awk_print
       except KeyError:
                awk_print = '2'
                return awk_print
'''

def check_target():
        sboxindicator='/targets/links/scratchbox.config'
        try :
                if os.path.islink(sboxindicator) and os.path.isfile(os.readlink(sboxindicator)) :
                        awk_print = '3'
                else:
                        awk_print = '2'
                return awk_print

        except OSError:
                awk_print = '2'
