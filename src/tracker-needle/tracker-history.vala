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

public class Tracker.History {
	private KeyFile data;
	private string filename;
	private string[] history;

	public History () {
		debug ("Loading history");

		data = new KeyFile ();
		filename = Path.build_filename (Environment.get_user_data_dir (), "tracker", "tracker-needle.txt", null);

		try {
			data.load_from_file (filename, KeyFileFlags.KEEP_COMMENTS | KeyFileFlags.KEEP_TRANSLATIONS);
		} catch (KeyFileError e1) {
			warning ("Could not load history from file:'%s': %s", filename, e1.message);
			return;
		} catch (FileError e2) {
			warning ("Could not load history from file:'%s': %s", filename, e2.message);
			return;
		}

		if (data.has_group ("History") == false) {
			debug ("  No history found");
			return;
		}

		try {
			history = data.get_string_list ("History", "criteria");
		} catch (KeyFileError e1) {
			warning ("Could not load history from file:'%s': %s", filename, e1.message);
			return;
		}

		debug ("  Found %d previous search histories", history.length);

		debug ("  Done");
	}

	~History () {
		debug ("Saving history");

		data.set_string_list ("History", "criteria", history);

		try {
			string output = data.to_data ();

			FileUtils.set_contents (filename, output, -1);
		} catch (GLib.FileError e1) {
			warning ("Could not save history to file:'%s': %s", filename, e1.message);
		}

		debug ("  Done");
	}

	public void add (string criteria)
	requires (criteria != null && criteria.length > 0) {
		// Don't add the same item more than once
		foreach (string check in history) {
			if (check == criteria) {
				return;
			}
		}

		history += criteria;
	}

	public string[] get () {
		return history;
	}

}
