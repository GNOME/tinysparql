#!/usr/bin/env python3
# Copyright (C) 2020, Sam Thursfield <sam@afuera.me.uk>
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


"""Demonstrates publishing a database over DBus."""

import gi
gi.require_version('Tracker', '3.0')
from gi.repository import Gio
from gi.repository import GLib
from gi.repository import Tracker

import logging
import os
import sys
import tempfile


def main():
    logging.basicConfig(stream=sys.stderr, level=logging.DEBUG)

    tmpdir = tempfile.mkdtemp(prefix='tracker-test-')

    # Where the database is stored.
    store_path = Gio.File.new_for_path(tmpdir)

    # The database schemas.
    ontology_path = Tracker.sparql_get_ontology_nepomuk()

    cancellable = None

    # Create a new, empty database.
    conn = Tracker.SparqlConnection.new(
        Tracker.SparqlConnectionFlags.NONE,
        store_path,
        ontology_path,
        cancellable)

    bus = Gio.bus_get_sync(Gio.BusType.SESSION, cancellable)
    unique_name = bus.get_unique_name()

    # Publish our endpoint on DBus.
    endpoint = Tracker.EndpointDBus.new(conn, bus, None, cancellable)

    print(f"Exposing a Tracker endpoint on bus name {unique_name}")
    print()
    print(f"Try connecting over D-Bus using `tracker3 sparql`:")
    print()
    print(f"   tracker3 sparql --dbus-service={unique_name} -q ...")

    loop = GLib.MainLoop.new(None, False)

    if os.environ.get('TRACKER_EXAMPLES_AUTOMATED_TEST'):
        GLib.timeout_add(10, lambda *args: loop.quit(), None)
    else:
        print()
        print(f"Press CTRL+C to quit.")

    loop.run()


main()
