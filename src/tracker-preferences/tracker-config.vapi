/*
 * Copyright (C) 2008-2009, Nokia
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
 *
 * Author: Philip Van Hoof <philip@codeminded.be>
 */

namespace Tracker {
	[CCode (cheader_filename = "miners/fs/tracker-config.h")]
	public class Config : GLib.Object {
		public Config ();

		public int verbosity { get; set; }
		public int initial_sleep { get; set; }
		public bool enable_monitors { get; set; }
		public int throttle { get; set; }
		public bool enable_thumbnails { get; set; }
		public bool index_on_battery { get; set; }
		public bool index_on_battery_first_time { get; set; }
		public bool index_removable_devices { get; set; }
		public bool index_optical_discs { get; set; }
		public bool index_mounted_directories { get; set; }
		public int low_disk_space_limit { get; set; }
		public int removable_days_threshold { get; set; }
		public GLib.SList<string> index_recursive_directories { get; set; }
		public GLib.SList<string> index_recursive_directories_unfiltered { get; }
		public GLib.SList<string> index_single_directories { get; set; }
		public GLib.SList<string> index_single_directories_unfiltered { get; }
		public GLib.SList<string> ignored_directories { get; set; }
		public GLib.SList<string> ignored_directories_with_content { get; set; }
		public GLib.SList<string> ignored_files { get; set; }
		public GLib.SList<string> ignored_directory_patterns { get; }
		public GLib.SList<string> ignored_file_patterns { get ; }

		public bool save ();
	}
}

