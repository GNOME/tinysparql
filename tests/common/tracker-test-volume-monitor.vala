/*
 * Copyright (C) 2012, Codethink Ltd <sam.thursfield@codethink.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/* WARNING: due to issues in gio-2.0.vapi, this file is really hard to
 * compile with Vala. See https://bugzilla.gnome.org/show_bug.cgi?id=677073
 * for more information. Use either 0.15.0 or >= 0.17.1
 */

private class Tracker.TestMount: GLib.Mount, GLib.Object {
	internal File root;
	internal string root_path;
	internal uint id;

	public TestMount (string root_path,
	                  uint   id) {
		this.root_path = root_path;
		this.id = id;
	}

	public File get_root () {
		if (root == null)
			// Can't create a GLib.File in the constructor due to locking in GIO
			this.root = File.new_for_path (root_path);

		return root;
	}

	public Icon get_icon () {
		return null;
	}

	public string get_name () {
		return "Test Mount %u".printf(id);
	}

	public File get_default_location () {
		return this.get_root ();
	}

	public string get_uuid () {
		return "FFFF-F%03u".printf(id);
	}

	public Volume get_volume () {
		return null;
	}

	public Drive get_drive () {
		return null;
	}

	public bool can_unmount () {
		return true;
	}

	public bool can_eject () {
		return false;
	}

	public async bool eject (GLib.MountUnmountFlags flags,
	                         GLib.Cancellable? cancellable)
	                        throws GLib.Error {
		return false;
	}

	public async bool eject_with_operation (GLib.MountUnmountFlags flags,
	                                        GLib.MountOperation? mount_operation,
	                                        GLib.Cancellable? cancellable)
	                                       throws GLib.Error {
		return false;
	}


	public string[] guess_content_type_sync (bool         force_rescan,
	                                         Cancellable? cancellable)
	                                        throws GLib.Error {
		return new string[1];
	}

	public async string[] guess_content_type (bool         force_rescan,
	                                          Cancellable? cancellable)
	                                         throws GLib.Error {
		return guess_content_type_sync (force_rescan, cancellable);
	}

	public async bool remount (GLib.MountMountFlags flags,
	                           GLib.MountOperation? mount_operation,
	                           GLib.Cancellable? cancellable)
	                           throws GLib.Error {
		return false;
	}

	public async bool unmount (MountUnmountFlags  flags,
	                           Cancellable?       cancellable)
	                           throws GLib.Error {
		return yield this.unmount_with_operation (flags, null, cancellable);
	}

	public async bool unmount_with_operation (MountUnmountFlags   flags,
	                                          MountOperation?     operation,
	                                          Cancellable?        cancellable)
	                                         throws GLib.Error {
		return false;
	}

#if GLIB_2_32
	public unowned string get_sort_key () {
		return null;
	}
#endif
}

public class Tracker.TestVolumeMonitor: GLib.VolumeMonitor {
	List<Mount> mount_list;
	uint dbus_mount_added_id;
	uint dbus_mount_removed_id;

	construct {
		// GObject construction is required because GUnionVolumeMonitor has to
		// use g_object_new() to create an instance.

		this.mount_list = null;

		Bus.watch_name (BusType.SESSION,
		                "org.freedesktop.Tracker1.TestMounter",
		                BusNameWatcherFlags.NONE,
		                on_test_mount_service_appeared,
		                on_test_mount_service_vanished);
	}

	void on_test_mount_service_appeared (DBusConnection connection,
	                                     string         name,
	                                     string         name_owner) {
		dbus_mount_added_id = connection.signal_subscribe
		    (name,
		     "org.freedesktop.Tracker1.TestMounter",
		     "MountAdded",
		     "/org/freedesktop/Tracker1/TestMounter",
		     null,
		     DBusSignalFlags.NONE,
		     dbus_mount_added_cb);

		dbus_mount_removed_id = connection.signal_subscribe
		    (name,
		     "org.freedesktop.Tracker1.TestMounter",
		     "MountRemoved",
		     "/org/freedesktop/Tracker1/TestMounter",
		     null,
		     DBusSignalFlags.NONE,
		     dbus_mount_removed_cb);

		connection.call.begin (name,
		                       "/org/freedesktop/Tracker1/TestMounter",
		                       "org.freedesktop.Tracker1.TestMounter",
		                       "ListMounts",
		                       null, null, DBusCallFlags.NONE, -1, null,
		                       (s, r) => {
			Variant reply;
 
			try {
				reply = connection.call.end (r);
			}
			catch (Error e) {
				warning ("Call to org.freedesktop.Tracker1.TestMounter.ListMounts() failed: %s", e.message);
				return;
			}

			string root_path;
			uint id;

			var mount_iter = reply.get_child_value(0).get_child_value(0).iterator ();
			while (mount_iter.next ("(su)", out root_path, out id))
				this.add_mount (root_path, id);
		});
	}

	void on_test_mount_service_vanished (DBusConnection connection,
	                                     string         name) {
		foreach (Mount m in mount_list)
			this.mount_removed (m);

		connection.signal_unsubscribe (this.dbus_mount_added_id);
		connection.signal_unsubscribe (this.dbus_mount_removed_id);

		this.mount_list = null;
	}

	unowned List<Mount>? find_mount_by_root_path (string root_path) {
		for (unowned List<Mount> l = this.mount_list; l != null; l = l.next)
			if (((Tracker.TestMount)l.data).root_path == root_path)
				return l;

		return null;
	}

	void add_mount (string root_path,
	                uint   id) {
		unowned List<Mount> l = find_mount_by_root_path (root_path);

		if (l != null)
			return;

		Mount mount = new Tracker.TestMount (root_path, id);

		this.mount_list.prepend (mount);

		this.mount_added (mount);
	}

	void remove_mount (string root_path) {
		unowned List<Mount> l = find_mount_by_root_path (root_path);

		if (l == null)
			return;

		Mount mount = l.data;
		this.mount_list.delete_link (l);
		this.mount_removed (mount);
	}

	void dbus_mount_added_cb (DBusConnection connection,
	                          string         sender_name,
	                          string         object_path,
	                          string         interface_name,
	                          string         signal_name,
	                          Variant        parameters) {
		string root_path;
		uint id;

		parameters.get ("(su)", out root_path, out id);
		this.add_mount (root_path, id);
	}

	void dbus_mount_removed_cb (DBusConnection connection,
	                            string         sender_name,
	                            string         object_path,
	                            string         interface_name,
	                            string         signal_name,
	                            Variant        parameters) {
		string root_path;

		parameters.get ("(s)", out root_path);
		this.remove_mount (root_path);
	}


	public override bool is_supported () {
		return true;
	}

	public override List<unowned Mount> get_mounts () {
		return this.mount_list.copy ();
	}

	public override List<Volume> get_volumes () {
		return null;
	}

	public override List<Drive> get_connected_drives () {
		return null;
	}

	public override Volume get_volume_for_uuid (string uuid) {
		return null;
	}

	public override Mount get_mount_for_uuid (string uuid) {
		return null;
	}

	public Mount? get_mount_for_mount_path (string mount_path) {
		foreach (Mount m in this.mount_list) {
			if (m.get_root().get_path() == mount_path)
				return m;
		}

		return null;
	}
}

public static void g_io_module_load (IOModule module) {
	tracker_test_volume_monitor_register_types (module);
}

public static void g_io_module_unload (IOModule module) {
}

[CCode (array_length = false)]
public static string[] g_io_module_query () {
	return {"gio-volume-monitor"};
}

[ModuleInit]
public static void tracker_test_volume_monitor_register_types (TypeModule module) {
	GLib.IOExtensionPoint.implement ("gio-volume-monitor",
	                                 typeof (Tracker.TestVolumeMonitor),
	                                 "tracker-test-volume-monitor",
	                                 0);
}

