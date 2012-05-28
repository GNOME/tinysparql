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

/* A slightly overengineed method of implementing test mounts. It might be
 * nicer if GVfs would support this directly, see:
 * https://bugzilla.gnome.org/show_bug.cgi?id=659739
 *
 * Tests can run this daemon and use it to control the
 * tracker-test-volume-monitor GIO module, creating fake mounts for all of the
 * Tracker processes.
 */

using GLib;

struct Tracker.TestMountInfo {
	public string root_path;
	public uint id;
}

[DBus (name="org.freedesktop.Tracker1.TestMounter")]
public class Tracker.TestMountService {
	List<TestMountInfo?> mount_list;
	uint id_counter;

	internal void on_bus_acquired (DBusConnection connection,
	                               string         name) {
		try {
			connection.register_object ("/org/freedesktop/Tracker1/TestMounter", this);
		}
		catch (IOError e) {
			stderr.printf ("tracker-test-volume-monitor: Error exporting on bus: %s\n", e.message);
		}
	}

	internal void on_name_acquired (DBusConnection connection,
	                                string         name) {
	}

	internal void on_name_lost (DBusConnection connection,
	                            string         name) {
		stderr.printf ("tracker-test-volume-monitor: Unable to own bus name\n");
	}

	public Variant list_mounts () {
		var result = new VariantBuilder ((VariantType) "a(su)");

		foreach (var mount_info in this.mount_list)
			result.add ("(su)", mount_info.root_path, mount_info.id);

		return result.end();
	}

	private unowned List<TestMountInfo?>? find_mount_info_node (string root_path) {
		for (unowned List<TestMountInfo?> l = this.mount_list; l != null; l = l.next) {
			if (l.data.root_path == root_path)
				return l;
		}

		return null;
	}

	public void mount (string root_path)
	                  throws GLib.IOError {
		unowned List<TestMountInfo?> node = this.find_mount_info_node (root_path);

		if (node != null)
			throw new IOError.ALREADY_MOUNTED ("Unable to mount %s: already mounted", root_path);

		var mount_info = TestMountInfo ();

		mount_info.root_path = root_path;
		mount_info.id = this.id_counter ++;

		this.mount_list.append (mount_info);

		this.mount_added (mount_info.root_path, mount_info.id);
	}

	public void unmount (string root_path)
	                    throws IOError {
		unowned List<TestMountInfo?> node = this.find_mount_info_node (root_path);

		if (node == null)
			throw new IOError.NOT_MOUNTED ("Unable to unmount %s: not mounted", root_path);

		var mount_info = node.data;

		this.mount_list.remove_link (node);

		this.mount_removed (mount_info.root_path);
	}

	public signal void mount_added (string root_path,
	                                uint   id);

	public signal void mount_removed (string root_path);
}

public int main (string args[]) {
	var loop = new MainLoop ();
	var service = new Tracker.TestMountService ();

	Bus.own_name (BusType.SESSION,
	              "org.freedesktop.Tracker1.TestMounter",
	              BusNameOwnerFlags.NONE,
	              service.on_bus_acquired,
	              service.on_name_acquired,
	              service.on_name_lost);

	loop.run ();

	return 0;
}
