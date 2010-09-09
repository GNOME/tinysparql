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

public class Tracker.View : ScrolledWindow {
	public enum Display {
		NO_RESULTS,
		CATEGORIES,
		FILE_LIST,
		FILE_ICONS
	}

	public Display display {
		get;
		private set;
	}

	public View (Display? _display = Display.NO_RESULTS) {
		Widget results = null;
		
		set_policy (PolicyType.NEVER, PolicyType.AUTOMATIC);

		display = _display;

		switch (display) {
		case Display.NO_RESULTS:
			Label l;

			l = new Label ("");

			string message = _("No Search Results");
			string markup = @"<big>$message</big>";
			
			l.set_use_markup (true);
			l.set_markup (markup);

			results = l;
			break;

		case Display.CATEGORIES:
		case Display.FILE_LIST:
			results = new TreeView ();
			break;

		case Display.FILE_ICONS:
			results = new IconView ();
			break;
		}

		if (display == Display.NO_RESULTS) {
			add_with_viewport (results);
		} else {
			add (results);
		}

		base.show_all ();
	}
}

