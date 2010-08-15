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

private const int secs_per_day = 60 * 60 * 24;

public string tracker_item_format_time (string s) {
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
		return t.format ("%x");
	}

	if (diff_days_abs == 0) {
		return "Today";
	} else {
		bool future = false;

		if (diff_days < 0)
			future = true;

		if (diff_days <= 1) {
			if (future)
				return "Tomorrow";
			else
				return "Yesterday";
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
