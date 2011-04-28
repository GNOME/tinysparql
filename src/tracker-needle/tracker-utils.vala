//
// Copyright 2010, Martyn Russell <martyn@lanedo.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301, USA.
//

using Gtk;

private const int secs_per_day = 60 * 60 * 24;

public string tracker_time_format_from_iso8601 (string s) {
	GLib.Time t = GLib.Time ();
	t.strptime (s, "%FT%T");

	var tv_now = GLib.TimeVal ();
	tv_now.get_current_time ();

	var tv_then = GLib.TimeVal ();
	tv_then.from_iso8601 (s);

	var diff_sec = tv_now.tv_sec - tv_then.tv_sec;
	var diff_days = diff_sec / secs_per_day;
	var diff_days_abs = diff_days.abs ();

	// stdout.printf ("timeval now:%ld, then:%ld, diff secs:%ld, diff days:%ld, abs: %ld, seconds per day:%d\n", tv_now.tv_sec, tv_then.tv_sec, diff_sec, diff_days, diff_days_abs, secs_per_day);

	// if it's more than a week, use the default date format
	if (diff_days_abs > 7) {
		/* Translators: This is a strftime(3) date format string, read its man page to fit your locale better */
		return t.format (_("%x"));
	}

	if (diff_days_abs == 0) {
		return _("Today");
	} else {
		bool future = false;

		if (diff_days < 0)
			future = true;

		if (diff_days <= 1) {
			if (future)
				return _("Tomorrow");
			else
				return _("Yesterday");
		} else {
			if (future) {
				/* Translators: %d is replaced with a number of days. It's always greater than 1 */
				return ngettext ("%ld day from now", "%ld days from now", diff_days_abs).printf (diff_days_abs);
			} else {
				/* Translators: %d is replaced with a number of days. It's always greater than 1 */
				return ngettext ("%ld day ago", "%ld days ago", diff_days_abs).printf (diff_days_abs);
			}
		}
	}
}

public string tracker_time_format_from_seconds (string seconds_str) {
	double seconds = seconds_str.to_int ();
	double total;
	int d, h, m, s;
	
	if (seconds == 0.0) {
		return _("Less than one second");
	}

	total = seconds;
	s = (int) total % 60;
	total /= 60;
	m = (int) total % 60;
	total /= 60;
	h = (int) total % 24;
	d = (int) total / 24;

	var output = new StringBuilder ("");

	if (d > 0) {
		output.append (" %dd".printf (d));
	}

	if (h > 0) {
		output.append (" %.2d".printf (h));
	}

	if (m > 0) {
		output.append ("%s%.2d".printf (h > 0 ? ":" : "", m));
	}

	// Always show seconds
	output.append ("%s%.2d".printf (m > 0 ? ":" : "0:", s));

	if (output.len < 1) {
		return _("Less than one second");
	}

	string str = output.str;

	return str._chug ();
}

public Gdk.Pixbuf tracker_pixbuf_new_from_file (IconTheme theme, string filename, int size, bool is_image) {
	// Get Icon
	var file = File.new_for_uri (filename);
	var pixbuf = null as Gdk.Pixbuf;

	if (is_image) {
		try {
			pixbuf = new Gdk.Pixbuf.from_file_at_size (file.get_path (), size, size);
		} catch (GLib.Error e) {
			warning ("Error loading icon pixbuf: " + e.message);
		}
		
		if (pixbuf != null) {
			return pixbuf;
		}
	}

	if (file.query_exists (null)) {
		try {
			var file_info = file.query_info ("standard::icon",
			                                 FileQueryInfoFlags.NONE,
			                                 null);

			if (file_info != null) {
				var icon = file_info.get_icon ();

				try {
					if (icon is FileIcon) {
						pixbuf = new Gdk.Pixbuf.from_file (((FileIcon) icon).get_file ().get_path ());
					} else if (icon is ThemedIcon) {
						pixbuf = theme.load_icon (((ThemedIcon) icon).get_names ()[0], size, Gtk.IconLookupFlags.USE_BUILTIN);
					}
				} catch (GLib.Error e) {
					warning ("Error loading icon pixbuf: " + e.message);
				}
			}
		} catch (GLib.Error e) {
			warning ("Error looking up file for pixbuf: " + e.message);
		}
	}

	if (pixbuf != null) {
		return pixbuf;
	}

	return tracker_pixbuf_new_from_name (theme, "text-x-generic", size);
}

public Gdk.Pixbuf tracker_pixbuf_new_from_name (IconTheme theme, string name, int size) {
	// Get Icon
	var pixbuf = null as Gdk.Pixbuf;

	try {
		pixbuf = theme.load_icon (name, size, IconLookupFlags.USE_BUILTIN);
	} catch (GLib.Error e) {
		warning ("Could not load default icon pixbuf from theme for '%s': %s", name, e.message);
	}

	return pixbuf;
}
