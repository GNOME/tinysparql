/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

using Gtk;
using GLib;
using Tracker;

[CCode (cname = "TRACKER_UI_DIR")]
extern static const string UIDIR;

[CCode (cname = "SRCDIR")]
extern static const string SRCDIR;

public class Tracker.Preferences {
	private GLib.Settings settings = null;

	private const string UI_FILE = "tracker-preferences.ui";
	private const string HOME_STRING = "$HOME";

	private Window window;
	private CheckButton checkbutton_enable_index_on_battery_first_time;
	private CheckButton checkbutton_enable_index_on_battery;
	private SpinButton spinbutton_delay;
	private CheckButton checkbutton_enable_monitoring;
	private CheckButton checkbutton_index_removable_media;
	private CheckButton checkbutton_index_optical_discs;
	private Scale hscale_disk_space_limit;
	private Scale hscale_throttle;
	private Scale hscale_drop_device_threshold;
	private ListStore liststore_index_recursively;
	private ListStore liststore_index_single;
	private ListStore liststore_ignored_directories;
	private ListStore liststore_ignored_files;
	private ListStore liststore_gnored_directories_with_content;
	private TreeView treeview_index_recursively;
	private TreeView treeview_index_single;
	private TreeView treeview_ignored_directories;
	private TreeView treeview_ignored_directories_with_content;
	private TreeView treeview_ignored_files;
	private ToggleButton togglebutton_home;
	private Notebook notebook;

	public Preferences () {
		debug ("Getting current settings");

		//config = new Config ();
		settings = new GLib.Settings ("org.freedesktop.Tracker.Miner.Files");

		// Change notification for any key in the schema
		settings.changed.connect ((key) => {
		      print ("Key '%s' changed\n", key);
		});
	}

	public void setup_ui () {
		var builder = new Gtk.Builder ();

		try {
			debug ("Trying to use UI file:'%s'", SRCDIR + UI_FILE);
			builder.add_from_file (SRCDIR + UI_FILE);
		} catch (GLib.Error e) {
			//now the install location
			try {
				debug ("Trying to use UI file:'%s'", UIDIR + UI_FILE);
				builder.add_from_file (UIDIR + UI_FILE);
			} catch (GLib.Error e) {
				var msg = new MessageDialog (null,
				                             DialogFlags.MODAL,
				                             MessageType.ERROR,
				                             ButtonsType.CANCEL,
				                             "Failed to load UI file, %s\n",
				                             e.message);
				msg.run ();
				Gtk.main_quit();
			}
		}

		// Get widgets from .ui file
		window = builder.get_object ("tracker-preferences") as Window;
		notebook = builder.get_object ("notebook") as Notebook;

		checkbutton_enable_monitoring = builder.get_object ("checkbutton_enable_monitoring") as CheckButton;
		checkbutton_enable_index_on_battery = builder.get_object ("checkbutton_enable_index_on_battery") as CheckButton;
		checkbutton_enable_index_on_battery_first_time = builder.get_object ("checkbutton_enable_index_on_battery_first_time") as CheckButton;
		spinbutton_delay = builder.get_object ("spinbutton_delay") as SpinButton;
		checkbutton_index_removable_media = builder.get_object ("checkbutton_index_removable_media") as CheckButton;
		checkbutton_index_optical_discs = builder.get_object ("checkbutton_index_optical_discs") as CheckButton;
		checkbutton_index_optical_discs.set_sensitive (checkbutton_index_removable_media.active);
		hscale_disk_space_limit = builder.get_object ("hscale_disk_space_limit") as Scale;
		hscale_throttle = builder.get_object ("hscale_throttle") as Scale;
		hscale_drop_device_threshold = builder.get_object ("hscale_drop_device_threshold") as Scale;
		togglebutton_home = builder.get_object ("togglebutton_home") as ToggleButton;

		treeview_index_recursively = builder.get_object ("treeview_index_recursively") as TreeView;
		treeview_index_single = builder.get_object ("treeview_index_single") as TreeView;
		treeview_ignored_directories = builder.get_object ("treeview_ignored_directories") as TreeView;
		treeview_ignored_directories_with_content = builder.get_object ("treeview_ignored_directories_with_content") as TreeView;
		treeview_ignored_files = builder.get_object ("treeview_ignored_files") as TreeView;

		setup_standard_treeview (treeview_index_recursively, _("Directory"));
		setup_standard_treeview (treeview_index_single, _("Directory"));
		setup_standard_treeview (treeview_ignored_directories, _("Directory"));
		setup_standard_treeview (treeview_ignored_directories_with_content, _("Directory"));
		setup_standard_treeview (treeview_ignored_files, _("File"));

		liststore_index_recursively = builder.get_object ("liststore_index_recursively") as ListStore;
		liststore_index_single = builder.get_object ("liststore_index_single") as ListStore;
		liststore_ignored_directories = builder.get_object ("liststore_ignored_directories") as ListStore;
		liststore_ignored_files = builder.get_object ("liststore_ignored_files") as ListStore;
		liststore_gnored_directories_with_content = builder.get_object ("liststore_gnored_directories_with_content") as ListStore;

		// Set initial values
		checkbutton_enable_index_on_battery.active = settings.get_boolean ("index-on-battery");
		checkbutton_enable_index_on_battery_first_time.set_sensitive (!checkbutton_enable_index_on_battery.active);
		checkbutton_enable_index_on_battery_first_time.active = settings.get_boolean ("index-on-battery-first-time");
		spinbutton_delay.set_increments (1, 1);
		spinbutton_delay.value = (double) settings.get_int ("initial-sleep");
		checkbutton_enable_monitoring.active = settings.get_boolean ("enable-monitors");
		checkbutton_index_removable_media.active = settings.get_boolean ("index-removable-devices");
		checkbutton_index_optical_discs.active = settings.get_boolean ("index-optical-discs");
		hscale_disk_space_limit.set_value ((double) settings.get_int ("low-disk-space-limit"));
		hscale_throttle.set_value ((double) settings.get_int ("throttle"));
		hscale_drop_device_threshold.set_value ((double) settings.get_int ("removable-days-threshold"));

		// FIXME: Work out how to do this
		fill_in_model (liststore_index_recursively, settings.get_strv ("index-recursive-directories"));
		fill_in_model (liststore_index_single, settings.get_strv ("index-single-directories"));
		fill_in_model (liststore_ignored_directories, settings.get_strv ("ignored-directories"));
		fill_in_model (liststore_ignored_files, settings.get_strv ("ignored-files"));
		fill_in_model (liststore_gnored_directories_with_content, settings.get_strv ("ignored-directories-with-content"));

		togglebutton_home.active = model_contains (liststore_index_recursively, HOME_STRING);

		// We hide this page because it contains the start up
		// delay which is not necessary to display for most people.
		notebook.remove_page (0);

		// Connect signals
		// builder.connect_signals (null);
		builder.connect_signals_full (connect_signals);
	}

	public void show () {
		setup_ui ();

		window.show ();
	}


	// This function is used to fix up the parameter ordering for callbacks
	// from the .ui file which has the callback names.
	[CCode (instance_pos = -1)]
	private void connect_signals (Gtk.Builder builder, GLib.Object object,
	                              string signal_name, string handler_name,
	                              GLib.Object? connect_object,
	                              GLib.ConnectFlags flags) {
		var module = Module.open (null, ModuleFlags.BIND_LAZY);
		void* sym;

		if (!module.symbol (handler_name, out sym)) {
			stdout.printf ("Symbol not found! %s\n", handler_name);
		} else {
			Signal.connect (object, signal_name, (GLib.Callback) sym, this);
		}
	}

	[CCode (instance_pos = -1)]
	public void response_cb (Dialog source, int response_id) {
		debug ("Got response id %d (apply:%d, close:%d)", response_id, ResponseType.APPLY, ResponseType.CLOSE);

		switch (response_id) {
		case ResponseType.APPLY:
			debug ("Converting directories for storage");

			// FIXME: Work out how to do this.
			settings.set_strv ("index-single-directories", model_to_strv (liststore_index_single));
			settings.set_strv ("index-recursive-directories", model_to_strv (liststore_index_recursively));
			settings.set_strv ("ignored-directories", model_to_strv (liststore_ignored_directories));
			settings.set_strv ("ignored-files", model_to_strv (liststore_ignored_files));
			settings.set_strv ("ignored-directories-with-content", model_to_strv (liststore_gnored_directories_with_content));

			settings.set_int ("low-disk-space-limit", (int) hscale_disk_space_limit.get_value ());
			settings.set_int ("throttle", (int) hscale_throttle.get_value ());
			settings.set_int ("removable-days-threshold", (int) hscale_drop_device_threshold.get_value ());

			debug ("Saving settings...");
			settings.apply ();
			debug ("Done");

			// TODO: restart the Application and Files miner (no idea how to cleanly do this atm)
			return;

		default:
			break;
		}

		Gtk.main_quit ();
	}

	[CCode (instance_pos = -1)]
	public void spinbutton_delay_value_changed_cb (SpinButton source) {
		settings.set_int ("initial-sleep", source.get_value_as_int ());
	}

	[CCode (instance_pos = -1)]
	public void checkbutton_enable_monitoring_toggled_cb (CheckButton source) {
		settings.set_boolean ("enable-monitors", source.active);
	}

	[CCode (instance_pos = -1)]
	public void checkbutton_enable_index_on_battery_toggled_cb (CheckButton source) {
		settings.set_boolean ("index-on-battery", source.active);
		checkbutton_enable_index_on_battery_first_time.set_sensitive (!source.active);
	}

	[CCode (instance_pos = -1)]
	public void checkbutton_enable_index_on_battery_first_time_toggled_cb (CheckButton source) {
		settings.set_boolean ("index-on-battery-first-time", source.active);
	}

	[CCode (instance_pos = -1)]
	public void checkbutton_index_removable_media_toggled_cb (CheckButton source) {
		settings.set_boolean ("index-removable-devices", source.active);
		checkbutton_index_optical_discs.set_sensitive (source.active);
	}

	[CCode (instance_pos = -1)]
	public void checkbutton_index_optical_discs_toggled_cb (CheckButton source) {
		settings.set_boolean ("index-optical-discs", source.active);
	}

	[CCode (instance_pos = -1)]
	public string hscale_disk_space_limit_format_value_cb (Scale source, double value) {
		if (((int) value) == -1) {
			return _("Disabled");
		}

		return _("%d%%").printf ((int) value);
	}

	[CCode (instance_pos = -1)]
	public string hscale_throttle_format_value_cb (Scale source, double value) {
		return _("%d/20").printf ((int) value);
	}

	[CCode (instance_pos = -1)]
	public string hscale_drop_device_threshold_format_value_cb (Scale source, double value) {
		if (((int) value) == 0) {
			return _("Disabled");
		}

		return _("%d").printf ((int) value);
	}

	[CCode (instance_pos = -1)]
	public void button_index_recursively_add_clicked_cb (Button source) {
		add_dir (liststore_index_recursively);
	}

	[CCode (instance_pos = -1)]
	public void button_index_recursively_remove_clicked_cb (Button source) {
		del_dir (treeview_index_recursively);
	}

	[CCode (instance_pos = -1)]
	public void button_index_single_remove_clicked_cb (Button source) {
		del_dir (treeview_index_single);
	}

	[CCode (instance_pos = -1)]
	public void button_index_single_add_clicked_cb (Button source) {
		add_dir (liststore_index_single);
	}

	[CCode (instance_pos = -1)]
	public void button_ignored_directories_globs_add_clicked_cb (Button source) {
		add_freevalue (liststore_ignored_directories);
	}

	[CCode (instance_pos = -1)]
	public void button_ignored_directories_add_clicked_cb (Button source) {
		add_dir (liststore_ignored_directories);
	}

	[CCode (instance_pos = -1)]
	public void button_ignored_directories_remove_clicked_cb (Button source) {
		del_dir (treeview_ignored_directories);
	}

	[CCode (instance_pos = -1)]
	public void button_ignored_directories_with_content_add_clicked_cb (Button source) {
		add_freevalue (liststore_gnored_directories_with_content);
	}

	[CCode (instance_pos = -1)]
	public void button_ignored_directories_with_content_remove_clicked_cb (Button source) {
		del_dir (treeview_ignored_directories_with_content);
	}

	[CCode (instance_pos = -1)]
	public void button_ignored_files_add_clicked_cb (Button source) {
		add_freevalue (liststore_ignored_files);
	}

	[CCode (instance_pos = -1)]
	public void button_ignored_files_remove_clicked_cb (Button source) {
		del_dir (treeview_ignored_files);
	}

	[CCode (instance_pos = -1)]
	public void togglebutton_home_toggled_cb (ToggleButton source) {
		if (source.active && !model_contains (liststore_index_recursively, HOME_STRING)) {
			TreeIter iter;
			liststore_index_recursively.append (out iter);
			var v = Value (typeof (string));
			v.set_string (HOME_STRING);
			liststore_index_recursively.set_value (iter, 0, v);
		}

		if (!source.active && model_contains (liststore_index_recursively, HOME_STRING)) {
			bool valid;
			TreeIter iter;

			valid = liststore_index_recursively.get_iter_first (out iter);
			while (valid) {
				Value value;
				liststore_index_recursively.get_value (iter, 0, out value);
				if (value.get_string () == HOME_STRING) {
					liststore_index_recursively.remove (iter);
					valid = liststore_index_recursively.get_iter_first (out iter);
				} else {
					valid = liststore_index_recursively.iter_next (ref iter);
				}
			}
		}
	}

	private void add_freevalue (ListStore model) {
		Dialog dialog;
		Entry entry;
		Container content_area;

		dialog = new Dialog.with_buttons (_("Enter value"),
		                                  window,
		                                  DialogFlags.DESTROY_WITH_PARENT,
		                                  Stock.CANCEL, ResponseType.CANCEL,
		                                  Stock.OK, ResponseType.ACCEPT);

		dialog.set_default_response(ResponseType.ACCEPT);
		content_area = (Container) dialog.get_content_area ();
		entry = new Entry ();
		entry.set_activates_default (true);
		entry.show ();
		content_area.add (entry);

		if (dialog.run () == ResponseType.ACCEPT) {
			string text = entry.get_text ();

			if (text != null && text != "") {
				TreeIter iter;
				model.append (out iter);
				var v = Value (typeof (string));
				v.set_string (text);
				model.set_value (iter, 0, v);
			}
		}

		dialog.destroy ();
	}

	private void add_dir (ListStore model) {
		FileChooserDialog dialog = new FileChooserDialog (_("Select directory"), window,
		                                                  FileChooserAction.SELECT_FOLDER,
		                                                  Stock.CANCEL, ResponseType.CANCEL,
		                                                  Stock.OK, ResponseType.ACCEPT);

		if (dialog.run () == ResponseType.ACCEPT) {
			TreeIter iter;
			File dir;

			dir = dialog.get_file ();

			model.append (out iter);
			var v = Value (typeof (string));
			v.set_string (dir.get_path());
			model.set_value (iter, 0, v);
		}

		dialog.destroy ();
	}

	private void del_dir (TreeView view) {
		List<TreePath> list;
		ListStore store;
		TreeModel model;

		TreeSelection selection = view.get_selection ();
		list= selection.get_selected_rows (out model);

		store = (ListStore) model;

		foreach (TreePath path in list) {
			TreeIter iter;
			if (model.get_iter (out iter, path)) {
				store.remove (iter);
			}
		}
	}

	private string[] model_to_strv (ListStore model) {
		string[] list = {};
		TreeIter iter;
		bool valid;

		for (valid = model.get_iter_first (out iter);
		     valid;
		     valid = model.iter_next (ref iter)) {
			Value value;

			model.get_value (iter, 0, out value);
			list += value.get_string ();
		}

		return list;
	}

	public bool model_contains (TreeModel model, string needle) {
		TreeIter iter;
		bool valid;

		for (valid = model.get_iter_first (out iter);
		     valid;
		     valid = model.iter_next (ref iter)) {
			Value value;

			model.get_value (iter, 0, out value);

			if (value.get_string () == needle) {
				return true;
			}
		}

		return false;
	}

	private void fill_in_model (ListStore model, string[] list) {
		int position = 0;

		foreach (string str in list) {
			try {
				model.insert_with_values (null,
				                          position++,
				                          0,
				                          Filename.to_utf8 (str,
				                                            -1,
				                                            null,
				                                            null));
			} catch (GLib.ConvertError e) {
				print ("Could not convert filename to UTF8: %s", e.message);
			}
		}
	}

	private void setup_standard_treeview (TreeView view, string title) {
		TreeViewColumn column = new TreeViewColumn.with_attributes (title,
		                                                            new CellRendererText (),
		                                                            "text", 0);
		view.append_column (column);
	}
}

static bool print_version = false;

const OptionEntry[] options = {
	{ "version",
	  'V',
	  0,
	  OptionArg.NONE,
	  ref print_version,
	  N_("Print version"),
	  null },
	{ null }
};

static int main (string[] args) {
	OptionContext context = new OptionContext (_("Desktop Search preferences"));

	try {
		context.set_help_enabled (true);
		context.add_main_entries (options, null);
		context.add_group (Gtk.get_option_group (true));
		context.parse (ref args);
	} catch (Error e) {
		printerr (e.message + "\n\n");
		printerr (context.get_help (true, null));
		return 1;
	}

	if (print_version) {
		string about = "";
		string license = "";

		about   += "Tracker " + Config.PACKAGE_VERSION + "\n";

		license += "This program is free software and comes without any warranty.\n";
		license += "It is licensed under version 2 or later of the General Public ";
		license += "License which can be viewed at:\n";
		license += "\n";
		license += "  http://www.gnu.org/licenses/gpl.txt\n";

		print ("\n" + about + "\n" + license + "\n");
		return 0;
	}

	Gtk.init (ref args);

	Intl.bindtextdomain (Config.GETTEXT_PACKAGE, Config.LOCALEDIR);
	Intl.bind_textdomain_codeset (Config.GETTEXT_PACKAGE, "UTF-8");
	Intl.textdomain (Config.GETTEXT_PACKAGE);

	Tracker.Preferences p = new Tracker.Preferences ();
	p.show();

	Gtk.main ();

	return 0;
}
