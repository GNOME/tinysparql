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

gi.require_version("Tracker", "3.0")
from gi.repository import GLib
from gi.repository import Gio
from gi.repository import Tracker

import unittest

import configuration
import fixtures


class TestPortal(fixtures.TrackerPortalTest):
    def test_01_forbidden(self):
        self.start_service("org.freedesktop.Inaccessible")
        self.assertRaises(
            GLib.Error,
            self.query,
            "org.freedesktop.Inaccessible",
            "select ?u { BIND (1 AS ?u) }",
        )

    def test_02_allowed(self):
        self.start_service("org.freedesktop.PortalTest")
        res = self.query("org.freedesktop.PortalTest", "select ?u { BIND (1 AS ?u) }")
        self.assertEqual(len(res), 1)
        self.assertEqual(res[0][0], "1")

    def test_03_graph_access(self):
        self.start_service("org.freedesktop.PortalTest")
        self.update(
            "org.freedesktop.PortalTest",
            "CREATE GRAPH tracker:Disallowed;"
            + "INSERT { GRAPH tracker:Disallowed { <http://example/a> a nfo:FileDataObject } };"
            + "CREATE GRAPH tracker:Allowed;"
            + "INSERT { GRAPH tracker:Allowed { <http://example/b> a nfo:FileDataObject } }",
        )
        res = self.query(
            "org.freedesktop.PortalTest", "select ?u { ?u a rdfs:Resource }"
        )
        self.assertEqual(len(res), 1)
        self.assertEqual(res[0][0], "http://example/b")

    def test_04_rows_cols(self):
        self.start_service("org.freedesktop.PortalTest")
        res = self.query(
            "org.freedesktop.PortalTest",
            "select ?a ?b { VALUES (?a ?b) { (1 2) (3 4) (5 6) } }",
        )
        self.assertEqual(len(res), 3)
        self.assertEqual(res[0][0], "1")
        self.assertEqual(res[0][1], "2")
        self.assertEqual(len(res[0]), 2)
        self.assertEqual(res[1][0], "3")
        self.assertEqual(res[1][1], "4")
        self.assertEqual(len(res[1]), 2)
        self.assertEqual(res[2][0], "5")
        self.assertEqual(res[2][1], "6")
        self.assertEqual(len(res[2]), 2)

    def __wait_for_notifier(self):
        """
        In the callback of the signals, there should be a self.loop.quit ()
        """
        self.timeout_id = GLib.timeout_add_seconds(
            configuration.DEFAULT_TIMEOUT, self.__timeout_on_idle
        )
        self.loop.run_checked()

    def __timeout_on_idle(self):
        self.loop.quit()
        self.fail(
            "Timeout, the signal never came after %i seconds!"
            % configuration.DEFAULT_TIMEOUT
        )

    def __notifier_event_cb(self, notifier, service, graph, events):
        self.notifier_events += events
        if self.timeout_id != 0:
            GLib.source_remove(self.timeout_id)
            self.timeout_id = 0
        self.loop.quit()

    def test_05_local_connection_notifier(self):
        self.start_service("org.freedesktop.PortalTest")

        self.notifier_events = []
        conn = self.create_local_connection()
        notifier = conn.create_notifier()
        notifier.connect("events", self.__notifier_event_cb)
        signalId = notifier.signal_subscribe(
            self.bus, "org.freedesktop.PortalTest", None, "tracker:Allowed"
        )
        signalId2 = notifier.signal_subscribe(
            self.bus, "org.freedesktop.PortalTest", None, "tracker:Disallowed"
        )

        self.update(
            "org.freedesktop.PortalTest",
            "INSERT { GRAPH tracker:Disallowed { <http://example/a> a nmm:MusicPiece } }",
        )
        self.update(
            "org.freedesktop.PortalTest",
            "INSERT { GRAPH tracker:Allowed { <http://example/b> a nmm:MusicPiece } }",
        )

        self.__wait_for_notifier()
        notifier.signal_unsubscribe(signalId)
        notifier.signal_unsubscribe(signalId2)

        # Only one event is expected, from the allowed graph
        self.assertEqual(len(self.notifier_events), 1)
        self.assertEqual(self.notifier_events[0].get_urn(), "http://example/b")
        conn.close()

    def test_06_id_access(self):
        self.start_service("org.freedesktop.PortalTest")
        self.update(
            "org.freedesktop.PortalTest",
            "CREATE GRAPH tracker:Allowed;"
            + "INSERT { GRAPH tracker:Allowed { <http://example/b> a nfo:FileDataObject } }",
        )
        res = self.query(
            "org.freedesktop.PortalTest",
            "select tracker:id(xsd:string) tracker:uri(1) { }",
        )
        self.assertEqual(len(res), 1)
        self.assertEqual(res[0][0], "0")
        self.assertEqual(res[0][1], None)

        res = self.query(
            "org.freedesktop.PortalTest",
            "select tracker:id(<http://example/b>) tracker:uri(tracker:id(<http://example/b>)) { }",
        )
        self.assertEqual(len(res), 1)
        self.assertNotEqual(res[0][0], "0")
        self.assertEqual(res[0][1], "http://example/b")

    def test_07_id_access_disallowed(self):
        self.start_service("org.freedesktop.PortalTest")

        # Insert resource into disallowed graph, ensure it is not visible
        self.update(
            "org.freedesktop.PortalTest",
            "CREATE GRAPH tracker:Disallowed;"
            + "INSERT { GRAPH tracker:Disallowed { <http://example/b> a nfo:FileDataObject } }",
        )
        res = self.query(
            "org.freedesktop.PortalTest",
            "select tracker:id(<http://example/b>) tracker:uri(tracker:id(<http://example/b>)) { }",
        )
        self.assertEqual(len(res), 1)
        self.assertEqual(res[0][0], "0")
        self.assertIsNone(res[0][1])

        # Insert same resource into allowed graph, ensure it is visible
        self.update(
            "org.freedesktop.PortalTest",
            "CREATE GRAPH tracker:Allowed;"
            + "INSERT { GRAPH tracker:Allowed { <http://example/b> a nfo:FileDataObject } }",
        )
        res = self.query(
            "org.freedesktop.PortalTest",
            "select tracker:id(<http://example/b>) tracker:uri(tracker:id(<http://example/b>)) { }",
        )
        self.assertEqual(len(res), 1)
        self.assertNotEqual(res[0][0], "0")
        self.assertEqual(res[0][1], "http://example/b")
        resourceId = res[0][0]

        # Delete resource from allowed graph, ensure it is not visible again
        self.update(
            "org.freedesktop.PortalTest",
            "DELETE { GRAPH tracker:Allowed { <http://example/b> a rdfs:Resource } }",
        )
        res = self.query(
            "org.freedesktop.PortalTest",
            "select tracker:id(<http://example/b>) tracker:uri(tracker:id(<http://example/b>)) tracker:uri("
            + str(resourceId)
            + ") { }",
        )
        self.assertEqual(len(res), 1)
        self.assertEqual(res[0][0], "0")
        self.assertIsNone(res[0][1])
        self.assertIsNone(res[0][2])

    def test_08_local_connection_service(self):
        self.start_service("org.freedesktop.PortalTest")

        self.notifier_events = []
        conn = self.create_local_connection()
        self.update(
            "org.freedesktop.PortalTest",
            "INSERT { GRAPH tracker:Disallowed { <http://example/a> a nmm:MusicPiece } }",
        )
        self.update(
            "org.freedesktop.PortalTest",
            "INSERT { GRAPH tracker:Allowed { <http://example/b> a nmm:MusicPiece } }",
        )

        # Only one resource is expected, from the allowed graph
        cursor = conn.query(
            "select ?u { SERVICE <dbus:org.freedesktop.PortalTest> { ?u a nmm:MusicPiece } }"
        )
        self.assertTrue(cursor.next())
        self.assertEqual(cursor.get_string(0)[0], "http://example/b")
        self.assertFalse(cursor.next())
        cursor.close()
        conn.close()

    # Test that all ways to specify a graph in the query hit a dead end
    def test_09_query_graphs(self):
        self.start_service("org.freedesktop.PortalTest")
        self.update(
            "org.freedesktop.PortalTest",
            "CREATE GRAPH tracker:Disallowed;"
            + "INSERT { GRAPH tracker:Disallowed { <http://example/a> a nfo:FileDataObject } }",
        )

        res = self.query(
            "org.freedesktop.PortalTest",
            "select ?u from tracker:Disallowed { ?u a rdfs:Resource }",
        )
        self.assertEqual(len(res), 0)

        res = self.query(
            "org.freedesktop.PortalTest",
            "select ?u { graph tracker:Disallowed { ?u a rdfs:Resource } }",
        )
        self.assertEqual(len(res), 0)

        res = self.query(
            "org.freedesktop.PortalTest",
            "constraint graph tracker:Disallowed select ?u { ?u a rdfs:Resource }",
        )
        self.assertEqual(len(res), 0)

    # Test that it is not possible to break through into other services
    def test_10_query_services(self):
        self.start_service("org.freedesktop.PortalTest")
        self.start_service("org.freedesktop.InaccessibleService")
        self.update(
            "org.freedesktop.PortalTest",
            "CREATE GRAPH tracker:Allowed;"
            + "INSERT { GRAPH tracker:Allowed { <http://example/a> a nfo:FileDataObject } }",
        )
        self.update(
            "org.freedesktop.InaccessibleService",
            "CREATE GRAPH tracker:Allowed;"
            + "INSERT { GRAPH tracker:Allowed { <http://example/b> a nfo:FileDataObject } }",
        )

        try:
            exception = None
            res = self.query(
                "org.freedesktop.PortalTest",
                "select ?u { service <dbus:org.freedesktop.InaccessibleService> { ?u a rdfs:Resource } }",
            )
        except Exception as e:
            exception = e
        finally:
            self.assertIsNotNone(exception)

        try:
            exception = None
            res = self.query(
                "org.freedesktop.InaccessibleService",
                "select ?u { ?u a rdfs:Resource }",
            )
        except Exception as e:
            exception = e
        finally:
            self.assertIsNotNone(exception)

    # Test that property paths resolve correctly across allowed
    # and disallowed graphs
    def test_11_query_property_paths(self):
        self.start_service("org.freedesktop.PortalTest")
        self.update(
            "org.freedesktop.PortalTest",
            "CREATE GRAPH tracker:Disallowed;"
            + "INSERT { GRAPH tracker:Disallowed { "
            + '  <http://example/a> a nfo:FileDataObject ; nfo:fileName "A" ; nie:interpretedAs <http://example/b1> .'
            + '  <http://example/b1> a nmm:MusicPiece ; nie:isStoredAs <http://example/a> ; nie:title "title2" } }',
        )

        # Test property paths with allowed/disallowed graphs in both ends
        res = self.query(
            "org.freedesktop.PortalTest",
            "select ?u ?t { ?u nie:interpretedAs/nie:title ?t }",
        )
        self.assertEqual(len(res), 0)

        res = self.query(
            "org.freedesktop.PortalTest",
            "select ?u ?fn { ?u nie:isStoredAs/nfo:fileName ?fn }",
        )
        self.assertEqual(len(res), 0)

        # Insert a resource in the allowed graph
        self.update(
            "org.freedesktop.PortalTest",
            "CREATE GRAPH tracker:Allowed;"
            + "INSERT { GRAPH tracker:Allowed { "
            + '  <http://example/a> a nfo:FileDataObject ; nfo:fileName "A" ; nie:interpretedAs <http://example/a1> .'
            + '  <http://example/a1> a nmm:MusicPiece ; nie:isStoredAs <http://example/a> ; nie:title "title1" } }',
        )

        # Try the queries again
        res = self.query(
            "org.freedesktop.PortalTest",
            "select ?u ?t { ?u nie:interpretedAs/nie:title ?t }",
        )
        self.assertEqual(len(res), 1)
        self.assertEqual(res[0][0], "http://example/a")
        self.assertEqual(res[0][1], "title1")

        res = self.query(
            "org.freedesktop.PortalTest",
            "select ?u ?fn { ?u nie:isStoredAs/nfo:fileName ?fn }",
        )
        self.assertEqual(len(res), 1)
        self.assertEqual(res[0][0], "http://example/a1")
        self.assertEqual(res[0][1], "A")

        res = self.query(
            "org.freedesktop.PortalTest",
            "select ?u ?fn { GRAPH tracker:Disallowed { ?u nie:isStoredAs/nfo:fileName ?fn } }",
        )
        self.assertEqual(len(res), 0)

        # Delete the allowed resource again
        self.update("org.freedesktop.PortalTest", "DROP GRAPH tracker:Allowed")

        # Query results should revert to the original values
        res = self.query(
            "org.freedesktop.PortalTest",
            "select ?u ?t { ?u nie:interpretedAs/nie:title ?t }",
        )
        self.assertEqual(len(res), 0)

        res = self.query(
            "org.freedesktop.PortalTest",
            "select ?u ?fn { ?u nie:isStoredAs/nfo:fileName ?fn }",
        )
        self.assertEqual(len(res), 0)

    # Test that property paths resolve correctly across allowed
    # and disallowed graphs
    def test_12_query_fts(self):
        self.start_service("org.freedesktop.PortalTest")
        self.update(
            "org.freedesktop.PortalTest",
            "CREATE GRAPH tracker:Disallowed;"
            + "INSERT { GRAPH tracker:Disallowed { "
            + "  <http://example/a> a nfo:FileDataObject ; nie:interpretedAs <http://example/b1> ."
            + '  <http://example/b1> a nmm:MusicPiece ; nie:isStoredAs <http://example/a> ; nie:title "apples and oranges" } }',
        )

        # Query for both keywords, they are expected to be non-visible
        res = self.query(
            "org.freedesktop.PortalTest", 'select ?u { ?u fts:match "apples" }'
        )
        self.assertEqual(len(res), 0)

        res = self.query(
            "org.freedesktop.PortalTest", 'select ?u { ?u fts:match "oranges" }'
        )
        self.assertEqual(len(res), 0)

        # Insert a resource in the allowed graph
        self.update(
            "org.freedesktop.PortalTest",
            "CREATE GRAPH tracker:Allowed;"
            + "INSERT { GRAPH tracker:Allowed { "
            + '  <http://example/a> a nfo:FileDataObject ; nfo:fileName "file name" ; nie:interpretedAs <http://example/a1> .'
            + '  <http://example/a1> a nmm:MusicPiece ; nie:isStoredAs <http://example/a> ; nie:title "apples" } }',
        )

        # Try the queries again, we should get a match from the allowed graph for 'apples'
        res = self.query(
            "org.freedesktop.PortalTest", 'select ?u { ?u fts:match "apples" }'
        )
        self.assertEqual(len(res), 1)
        self.assertEqual(res[0][0], "http://example/a1")

        res = self.query(
            "org.freedesktop.PortalTest", 'select ?u { ?u fts:match "oranges" }'
        )
        self.assertEqual(len(res), 0)

        res = self.query(
            "org.freedesktop.PortalTest",
            'select ?u { GRAPH tracker:Disallowed { ?u fts:match "oranges" } }',
        )
        self.assertEqual(len(res), 0)

        res = self.query(
            "org.freedesktop.PortalTest",
            'select ?u { GRAPH tracker:Allowed { ?u fts:match "apples" } }',
        )
        self.assertEqual(len(res), 1)

        # Delete the allowed resource again
        self.update("org.freedesktop.PortalTest", "DROP GRAPH tracker:Allowed")

        # The query results should revert to the original values
        res = self.query(
            "org.freedesktop.PortalTest", 'select ?u { ?u fts:match "apples" }'
        )
        self.assertEqual(len(res), 0)

        res = self.query(
            "org.freedesktop.PortalTest", 'select ?u { ?u fts:match "oranges" }'
        )
        self.assertEqual(len(res), 0)

    # Test that property paths resolve correctly across allowed
    # and disallowed graphs
    def test_13_query_unrestricted_triples(self):
        self.start_service("org.freedesktop.PortalTest")
        self.update(
            "org.freedesktop.PortalTest",
            "CREATE GRAPH tracker:Disallowed;"
            + "INSERT { GRAPH tracker:Disallowed { "
            + '  <http://example/a> a nfo:FileDataObject ; nfo:fileName "A" . } }',
        )

        res = self.query("org.freedesktop.PortalTest", 'select ?s { ?s ?p "A" }')
        self.assertEqual(len(res), 0)

        res = self.query(
            "org.freedesktop.PortalTest", 'ASK { <http://example/a> ?p "A" }'
        )
        self.assertEqual(len(res), 1)
        self.assertNotEqual(res[0][0], "true")

        # Insert a resource in the allowed graph
        self.update(
            "org.freedesktop.PortalTest",
            "CREATE GRAPH tracker:Allowed;"
            + "INSERT { GRAPH tracker:Allowed { "
            + '  <http://example/a> a nfo:FileDataObject ; nfo:fileName "A" . } }',
        )

        # Try the queries again
        res = self.query("org.freedesktop.PortalTest", 'select ?s { ?s ?p "A" }')
        self.assertEqual(len(res), 1)
        self.assertEqual(res[0][0], "http://example/a")

        res = self.query(
            "org.freedesktop.PortalTest", 'ASK { <http://example/a> ?p "A" }'
        )
        self.assertEqual(len(res), 1)
        self.assertEqual(res[0][0], "true")

        res = self.query(
            "org.freedesktop.PortalTest",
            'select ?s { GRAPH tracker:Disallowed { ?s ?p "A" } }',
        )
        self.assertEqual(len(res), 0)

        res = self.query(
            "org.freedesktop.PortalTest",
            'select ?g { GRAPH ?g { ?s ?p "A" } }',
        )
        self.assertEqual(len(res), 1)

        # Delete the allowed resource again
        self.update("org.freedesktop.PortalTest", "DROP GRAPH tracker:Allowed")

        # The query results should revert to the original values
        res = self.query("org.freedesktop.PortalTest", 'select ?s { ?s ?p "A" }')
        self.assertEqual(len(res), 0)


    # Test that updates are forbidden
    def test_14_updates(self):
        self.start_service("org.freedesktop.PortalTest")
        self.update(
            "org.freedesktop.PortalTest",
            "CREATE GRAPH tracker:Allowed;"
            + "INSERT { GRAPH tracker:Allowed { "
            + "  <http://example/a> a nfo:FileDataObject ; nfo:fileName 'A' . } }",
        )

        res = self.query("org.freedesktop.PortalTest", 'select ?s { ?s ?p "A" }')
        self.assertEqual(len(res), 1)

        conn = Tracker.SparqlConnection.bus_new("org.freedesktop.PortalTest", None, self.bus)

        with self.assertRaisesRegex(Exception, 'not allowed') as exception:
            conn.update("INSERT { GRAPH tracker:Allowed { "
                        + "  <http://example/b> a nfo:FileDataObject ; nfo:fileName 'A' . } }")
        self.assertIsNotNone(exception)

        with self.assertRaisesRegex(Exception, 'not allowed') as exception:
            conn.update_blank("INSERT { GRAPH tracker:Allowed { "
                              + "  <http://example/c> a nfo:FileDataObject ; nfo:fileName 'A' . } }")
        self.assertIsNotNone(exception)

        res = self.query("org.freedesktop.PortalTest", 'select ?s { ?s ?p "A" }')
        self.assertEqual(len(res), 1)

    # Test that closing asynchronously the connection works
    def test_15_close_async(self):
        self.start_service("org.freedesktop.PortalTest")

        conn = Tracker.SparqlConnection.bus_new("org.freedesktop.PortalTest", None, self.bus)

        with self.assertRaisesRegex(Exception, 'not allowed') as exception:
            conn.update("INSERT { GRAPH tracker:Allowed { "
                        + "  <http://example/b> a nfo:FileDataObject ; nfo:fileName 'A' . } }")
        self.assertIsNotNone(exception)

        context = GLib.MainContext.new()
        context.push_thread_default()
        loop = GLib.MainLoop.new(context, False)
        self._connIsClosed = False

        def close_async_cb(conn, res):
            self._connIsClosed = conn.close_finish(res)
            loop.quit()

        conn.close_async(None, close_async_cb)
        loop.run()
        context.pop_thread_default()
        self.assertEqual(self._connIsClosed, True)


if __name__ == "__main__":
    fixtures.tracker_test_main()
