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
 */

namespace Tracker {
	[CCode (cheader_filename = "libtracker-common/tracker-date-time.h")]
	public int string_to_date (string date_string, out int offset) throws DateError;

	[CCode (cheader_filename = "libtracker-common/tracker-date-time.h")]
	public errordomain DateError {
		OFFSET,
		INVALID_ISO8601
	}

	[CCode (cheader_filename = "libtracker-common/tracker-common.h")]
	public class ConfigFile : GLib.Object {
		[NoAccessorMethod]
		public string domain { get; construct; }
		public bool save ();
		public virtual signal void changed ();
		public GLib.File file;
		public GLib.FileMonitor monitor;
		public bool file_exists;
		public GLib.KeyFile key_file;
	}

	[CCode (cheader_filename = "libtracker-common/tracker-common.h")]
	public class KeyfileObject {
		public static string blurb (void *object, string property);
		public static bool default_boolean (void *object, string property);
		public static int  default_int (void *object, string property);
		public static bool validate_int (void *object, string propery, int value);
		public static void load_int (void *object, string property, GLib.KeyFile key_file, string group, string key);
		public static void load_boolean (void *object, string property, GLib.KeyFile key_file, string group, string key);
		public static void load_string (void *object, string property, GLib.KeyFile key_file, string group, string key);
		public static void load_string_list (void *object, string property, GLib.KeyFile key_file, string group, string key, out GLib.SList return_instead);
		public static void load_directory_list (void *object, string property, GLib.KeyFile key_file, string group, string key, bool is_recursive, out GLib.SList return_instead);
		public static void save_int (void *object, string property, GLib.KeyFile key_file, string group, string key);
		public static void save_boolean (void *object, string property, GLib.KeyFile key_file, string group, string key);
		public static void save_string (void *object, string property, GLib.KeyFile key_file, string group, string key);
		public static void save_string_list (void *object, string property, GLib.KeyFile key_file, string group, string key);
		public static void save_directory_list (void *object, string property, GLib.KeyFile key_file, string group, string key);
	}
}

