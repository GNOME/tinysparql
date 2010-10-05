/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

[CCode (cheader_filename = "tracker-bus-fd-update.h")]
public void tracker_bus_fd_sparql_update (DBus.Connection connection, string query) throws Tracker.Sparql.Error, DBus.Error, GLib.IOError;
[CCode (cheader_filename = "tracker-bus-fd-update.h")]
public extern GLib.Variant tracker_bus_fd_sparql_update_blank (DBus.Connection connection, string query) throws Tracker.Sparql.Error, DBus.Error, GLib.IOError;
[CCode (cheader_filename = "tracker-bus-fd-update.h")]
public void tracker_bus_fd_sparql_batch_update (DBus.Connection connection, string query) throws Tracker.Sparql.Error, DBus.Error, GLib.IOError;
[CCode (cheader_filename = "tracker-bus-fd-update.h")]
public extern async void tracker_bus_fd_sparql_update_async (DBus.Connection connection, string query, GLib.Cancellable? cancellable = null) throws Tracker.Sparql.Error, DBus.Error, GLib.IOError;
[CCode (cheader_filename = "tracker-bus-fd-update.h")]
public extern async GLib.Variant tracker_bus_fd_sparql_update_blank_async (DBus.Connection connection, string query, GLib.Cancellable? cancellable = null) throws Tracker.Sparql.Error, DBus.Error, GLib.IOError;
[CCode (cheader_filename = "tracker-bus-fd-update.h")]
public extern async void tracker_bus_fd_sparql_batch_update_async (DBus.Connection connection, string query, GLib.Cancellable? cancellable = null) throws Tracker.Sparql.Error, DBus.Error, GLib.IOError;

[CCode (cheader_filename = "tracker-bus-fd-update.h")]
public extern async GLib.PtrArray? tracker_bus_fd_sparql_update_array_async (DBus.Connection connection, string query[], GLib.Cancellable? cancellable = null);
[CCode (cheader_filename = "tracker-bus-fd-update.h")]
public extern async GLib.PtrArray? tracker_bus_fd_sparql_batch_update_array_async (DBus.Connection connection, string[] query, GLib.Cancellable? cancellable = null);
