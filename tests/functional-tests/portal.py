# Copyright (C) 2020, Carlos Garnacho <carlosg@gnome.org>
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
Test portal
"""

from gi.repository import GLib

import unittest

import configuration
import fixtures

class TestPortal(fixtures.TrackerPortalTest):
    def test_01_forbidden(self):
        self.start_service('org.freedesktop.Inaccessible')
        self.assertRaises(
            GLib.Error, self.query,
            'org.freedesktop.Inaccessible',
            'select ?u { BIND (1 AS ?u) }')

    def test_02_allowed(self):
        self.start_service('org.freedesktop.PortalTest')
        res = self.query(
            'org.freedesktop.PortalTest',
            'select ?u { BIND (1 AS ?u) }')
        self.assertEqual(len(res), 1)
        self.assertEqual(res[0][0], '1')

    def test_03_graph_access(self):
        self.start_service('org.freedesktop.PortalTest')
        self.update(
            'org.freedesktop.PortalTest',
            'CREATE GRAPH tracker:Disallowed;' +
            'INSERT { GRAPH tracker:Disallowed { <a> a nfo:FileDataObject } };' +
            'CREATE GRAPH tracker:Allowed;' +
            'INSERT { GRAPH tracker:Allowed { <b> a nfo:FileDataObject } }')
        res = self.query(
            'org.freedesktop.PortalTest',
            'select ?u { ?u a rdfs:Resource }')
        self.assertEqual(len(res), 1)
        self.assertEqual(res[0][0], 'b')


if __name__ == '__main__':
    fixtures.tracker_test_main()
