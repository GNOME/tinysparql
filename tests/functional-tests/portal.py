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

import gi
gi.require_version('Tracker', '3.0')
from gi.repository import GLib
from gi.repository import Gio
from gi.repository import Tracker

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

    def test_04_rows_cols(self):
        self.start_service('org.freedesktop.PortalTest')
        res = self.query(
            'org.freedesktop.PortalTest',
            'select ?a ?b { VALUES (?a ?b) { (1 2) (3 4) (5 6) } }')
        self.assertEqual(len(res), 3)
        self.assertEqual(res[0][0], '1')
        self.assertEqual(res[0][1], '2')
        self.assertEqual(len(res[0]), 2)
        self.assertEqual(res[1][0], '3')
        self.assertEqual(res[1][1], '4')
        self.assertEqual(len(res[1]), 2)
        self.assertEqual(res[2][0], '5')
        self.assertEqual(res[2][1], '6')
        self.assertEqual(len(res[2]), 2)

    def __wait_for_notifier(self):
        """
        In the callback of the signals, there should be a self.loop.quit ()
        """
        self.timeout_id = GLib.timeout_add_seconds(
            configuration.DEFAULT_TIMEOUT, self.__timeout_on_idle)
        self.loop.run_checked()

    def __timeout_on_idle(self):
        self.loop.quit()
        self.fail("Timeout, the signal never came after %i seconds!" % configuration.DEFAULT_TIMEOUT)

    def __notifier_event_cb(self, notifier, service, graph, events):
        if self.timeout_id != 0:
            GLib.source_remove(self.timeout_id)
            self.timeout_id = 0
        self.loop.quit()

    def test_05_local_connection_notifier(self):
        self.start_service('org.freedesktop.PortalTest')

        conn = self.create_local_connection()
        notifier = conn.create_notifier();
        notifier.connect('events', self.__notifier_event_cb)
        signalId = notifier.signal_subscribe(
            self.bus,
            'org.freedesktop.PortalTest',
            None,
            'tracker:Allowed')

        self.update(
            'org.freedesktop.PortalTest',
            'INSERT { GRAPH tracker:Allowed { <b> a nmm:MusicPiece } }')

        self.__wait_for_notifier()
        notifier.signal_unsubscribe(signalId);
        conn.close()

        res = self.query(
            'org.freedesktop.PortalTest',
            'select ?a { ?a a nmm:MusicPiece }')
        self.assertEqual(len(res), 1)
        self.assertEqual(res[0][0], 'b')

if __name__ == '__main__':
    fixtures.tracker_test_main()
