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
	private GLib.Settings settings_fts = null;
	private GLib.Settings settings_miner_fs = null;
	private GLib.Settings settings_extract = null;

	private bool suggest_reindex = false;
	private bool suggest_restart = false;

	private const string UI_FILE = "tracker-preferences.ui";
	private const string HOME_STRING = "$HOME";
	private string HOME_STRING_EVALUATED;

	private UserDirectory[] ignored_user_directories = null;

	private Window window;
	private CheckButton checkbutton_enable_index_on_battery_first_time;
	private CheckButton checkbutton_enable_index_on_battery;
	private SpinButton spinbutton_delay;
	private CheckButton checkbutton_enable_monitoring;
	private CheckButton checkbutton_index_removable_media;
	private CheckButton checkbutton_index_optical_discs;
	private Scale hscale_disk_space_limit;
	private RadioButton radiobutton_sched_idle_always;
	private RadioButton radiobutton_sched_idle_first_index;
	private RadioButton radiobutton_sched_idle_never;
	private Scale hscale_drop_device_threshold;
	private ListStore liststore_index;
	private ListStore liststore_ignored_directories;
	private ListStore liststore_ignored_files;
	private ListStore liststore_ignored_directories_with_content;
	private TreeView treeview_index;
	private TreeView treeview_ignored_directories;
	private TreeView treeview_ignored_directories_with_content;
	private TreeView treeview_ignored_files;
	private TreeViewColumn treeviewcolumn_index1;
	private TreeViewColumn treeviewcolumn_index2;
	private ToggleButton togglebutton_home;
	private ToggleButton togglebutton_desktop;
	private ToggleButton togglebutton_documents;
	private ToggleButton togglebutton_music;
	private ToggleButton togglebutton_pictures;
	private ToggleButton togglebutton_videos;
	private ToggleButton togglebutton_download;
	private CheckButton checkbutton_index_file_content;
	private CheckButton checkbutton_index_numbers;
	private Box hbox_duplicate_warning;
	private Button button_reindex;
	private Notebook notebook;

	public Preferences () {
		debug ("Getting current settings");

		HOME_STRING_EVALUATED = dir_from_config (HOME_STRING);

		settings_fts = new GLib.Settings ("org.freedesktop.Tracker.FTS");
		/* settings_fts.delay(); */

		settings_miner_fs = new GLib.Settings ("org.freedesktop.Tracker.Miner.Files");
		/* settings_miner_fs.delay(); */

		settings_extract = new GLib.Settings ("org.freedesktop.Tracker.Extract");
		/* settings_extract.delay(); */

		// Change notification for any key in the schema
		settings_fts.changed.connect ((key) => {
		      print ("tracker-fts: Key '%s' changed\n", key);
		});

		settings_miner_fs.changed.connect ((key) => {
		      print ("tracker-miner-fs: Key '%s' changed\n", key);
		});

		settings_extract.changed.connect ((key) => {
		      print ("tracker-extract: Key '%s' changed\n", key);
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
		radiobutton_sched_idle_always = builder.get_object ("radiobutton_sched_idle_always") as RadioButton;
		radiobutton_sched_idle_first_index = builder.get_object ("radiobutton_sched_idle_first_index") as RadioButton;
		radiobutton_sched_idle_never = builder.get_object ("radiobutton_sched_idle_never") as RadioButton;
		hscale_drop_device_threshold = builder.get_object ("hscale_drop_device_threshold") as Scale;
		togglebutton_home = builder.get_object ("togglebutton_home") as ToggleButton;
		togglebutton_desktop = builder.get_object ("togglebutton_desktop") as ToggleButton;
		togglebutton_documents = builder.get_object ("togglebutton_documents") as ToggleButton;
		togglebutton_music = builder.get_object ("togglebutton_music") as ToggleButton;
		togglebutton_pictures = builder.get_object ("togglebutton_pictures") as ToggleButton;
		togglebutton_videos = builder.get_object ("togglebutton_videos") as ToggleButton;
		togglebutton_download = builder.get_object ("togglebutton_download") as ToggleButton;
		checkbutton_index_file_content = builder.get_object ("checkbutton_index_file_content") as CheckButton;
		checkbutton_index_numbers = builder.get_object ("checkbutton_index_numbers") as CheckButton;
		hbox_duplicate_warning = builder.get_object ("hbox_duplicate_warning") as Box;

		button_reindex = builder.get_object ("button_reindex") as Button;

		treeview_index = builder.get_object ("treeview_index") as TreeView;
		treeviewcolumn_index1 = builder.get_object ("treeviewcolumn_index1") as TreeViewColumn;
		treeviewcolumn_index2 = builder.get_object ("treeviewcolumn_index1") as TreeViewColumn;
		treeview_ignored_directories = builder.get_object ("treeview_ignored_directories") as TreeView;
		treeview_ignored_directories_with_content = builder.get_object ("treeview_ignored_directories_with_content") as TreeView;
		treeview_ignored_files = builder.get_object ("treeview_ignored_files") as TreeView;

		treeview_setup (treeview_index, _("Directory"), true, false);
		treeview_setup (treeview_ignored_directories, _("Directory"), false, true);
		treeview_setup (treeview_ignored_directories_with_content, _("Directory"), false, true);
		treeview_setup (treeview_ignored_files, _("File"), false, true);

		liststore_index = builder.get_object ("liststore_index") as ListStore;
		liststore_index.set_sort_column_id (0, Gtk.SortType.ASCENDING);
		liststore_ignored_directories = builder.get_object ("liststore_ignored_directories") as ListStore;
		liststore_ignored_files = builder.get_object ("liststore_ignored_files") as ListStore;
		liststore_ignored_directories_with_content = builder.get_object ("liststore_ignored_directories_with_content") as ListStore;

		// Set initial values
		checkbutton_enable_index_on_battery.active = settings_miner_fs.get_boolean ("index-on-battery");
		checkbutton_enable_index_on_battery_first_time.set_sensitive (!checkbutton_enable_index_on_battery.active);
		checkbutton_enable_index_on_battery_first_time.active = settings_miner_fs.get_boolean ("index-on-battery-first-time");
		spinbutton_delay.set_increments (1, 1);
		spinbutton_delay.value = (double) settings_miner_fs.get_int ("initial-sleep");
		checkbutton_enable_monitoring.active = settings_miner_fs.get_boolean ("enable-monitors");
		checkbutton_index_removable_media.active = settings_miner_fs.get_boolean ("index-removable-devices");
		checkbutton_index_optical_discs.set_sensitive (checkbutton_index_removable_media.active);
		checkbutton_index_optical_discs.active = settings_miner_fs.get_boolean ("index-optical-discs");
		hscale_disk_space_limit.set_value ((double) settings_miner_fs.get_int ("low-disk-space-limit"));
		hscale_drop_device_threshold.set_value ((double) settings_miner_fs.get_int ("removable-days-threshold"));

		// What do we do here if extract/miner-fs are different, we
		// could use inconsistent states for radiobuttons, but instead
		// we're going to just assume miner-fs is the lead here and
		// overwrite the extract config with anything we change here.
		int sched_idle = settings_miner_fs.get_enum ("sched-idle");

		if (sched_idle == 0) {
			radiobutton_sched_idle_always.active = true;
		} else if (sched_idle == 1) {
			radiobutton_sched_idle_first_index.active = true;
		} else if (sched_idle == 2) {
			radiobutton_sched_idle_never.active = true;
		} else {
			// If broken value set, use default.
			radiobutton_sched_idle_first_index.active = true;
		}

		// Evaluate any user directories which have same target directory
		sanitize_user_dirs ();

		// Populate and toggle user directories
		model_populate (liststore_index, settings_miner_fs.get_strv ("index-recursive-directories"), true, true);
		model_populate (liststore_index, settings_miner_fs.get_strv ("index-single-directories"), true, false);
		model_populate (liststore_ignored_directories, settings_miner_fs.get_strv ("ignored-directories"), false, false);
		model_populate (liststore_ignored_files, settings_miner_fs.get_strv ("ignored-files"), false, false);
		model_populate (liststore_ignored_directories_with_content, settings_miner_fs.get_strv ("ignored-directories-with-content"), false, false);

		if (ignored_user_directories.length > 0) {
			hbox_duplicate_warning.show ();
		} else {
			hbox_duplicate_warning.hide ();
		}

		togglebutton_home.active = model_contains (liststore_index, HOME_STRING_EVALUATED);
		togglebutton_desktop.active = model_contains (liststore_index, "&DESKTOP");
		togglebutton_documents.active = model_contains (liststore_index, "&DOCUMENTS");
		togglebutton_music.active = model_contains (liststore_index, "&MUSIC");
		togglebutton_pictures.active = model_contains (liststore_index, "&PICTURES");
		togglebutton_videos.active = model_contains (liststore_index, "&VIDEOS");
		togglebutton_download.active = model_contains (liststore_index, "&DOWNLOAD");

		checkbutton_index_file_content.active = settings_fts.get_int ("max-words-to-index") > 0;
		checkbutton_index_numbers.active = settings_fts.get_boolean ("ignore-numbers") != true;

		// Connect signals
		// builder.connect_signals (null);
		builder.connect_signals_full (connect_signals);
	}

	public void show () {
		setup_ui ();

		window.show ();
	}

	void reindex () {
		stdout.printf ("Reindexing...\n");

		string output, errors;
		int status;

		try {
			Process.spawn_sync (null, /* working dir */
			                    {"tracker-control", "--hard-reset", "--start" },
			                    null, /* env */
			                    SpawnFlags.SEARCH_PATH,
			                    null,
			                    out output,
			                    out errors,
			                    out status);
		} catch (GLib.Error e) {
			stderr.printf ("Could not reindex: %s", e.message);
		}
		stdout.printf ("%s\n", output);
		stdout.printf ("Finishing...\n");
	}

	void restart () {
		stdout.printf ("Restarting...\n");

		string output, errors;
		int status;

		try {
			Process.spawn_sync (null, /* working dir */
			                    {"tracker-control", "--terminate=miners", "--terminate=store", "--start" },
			                    null, /* env */
			                    SpawnFlags.SEARCH_PATH,
			                    null,
			                    out output,
			                    out errors,
			                    out status);
		} catch (GLib.Error e) {
			stderr.printf ("Could not restart: %s", e.message);
		}
		stdout.printf ("%s\n", output);
		stdout.printf ("Finishing...\n");
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

			settings_miner_fs.set_strv ("index-single-directories", model_to_strv (liststore_index, true, false));
			settings_miner_fs.set_strv ("index-recursive-directories", model_to_strv (liststore_index, true, true));
			settings_miner_fs.set_strv ("ignored-directories", model_to_strv (liststore_ignored_directories, false, false));
			settings_miner_fs.set_strv ("ignored-files", model_to_strv (liststore_ignored_files, false, false));
			settings_miner_fs.set_strv ("ignored-directories-with-content", model_to_strv (liststore_ignored_directories_with_content, false, false));

			settings_miner_fs.set_int ("low-disk-space-limit", (int) hscale_disk_space_limit.get_value ());
			settings_miner_fs.set_int ("removable-days-threshold", (int) hscale_drop_device_threshold.get_value ());

			int sched_idle;

			if (radiobutton_sched_idle_always.active) {
				sched_idle = 0;
			} else if (radiobutton_sched_idle_first_index.active) {
				sched_idle = 1;
			} else if (radiobutton_sched_idle_never.active) {
				sched_idle = 2;
			} else {
				assert_not_reached ();
			}

			// What do we do here if extract/miner-fs are different, we
			// could use inconsistent states for radiobuttons, but instead
			// we're going to just assume miner-fs is the lead here and
			// overwrite the extract config with anything we change here.
			settings_miner_fs.set_enum ("sched-idle", sched_idle);
			settings_extract.set_enum ("sched-idle", sched_idle);

			debug ("Saving settings...");
			settings_fts.apply ();
			debug ("  tracker-fts: Done");
			settings_miner_fs.apply ();
			debug ("  tracker-miner-fs: Done");
			settings_extract.apply ();
			debug ("  tracker-extract: Done");

			if (suggest_reindex) {
				Dialog dialog = new MessageDialog (window,
				                                   DialogFlags.DESTROY_WITH_PARENT,
				                                   MessageType.QUESTION,
				                                   ButtonsType.NONE,
				                                   "%s\n\n%s\n\n%s",
				                                   _("The changes you have made to your preferences here require a reindex to ensure all your data is correctly indexed as you have requested."),
				                                   _("This will close this dialog!"),
				                                   _("Would you like to reindex now?"),
				                                   null);
				dialog.add_buttons (_("Reindex"), ResponseType.YES,
				                    _("Do nothing"), ResponseType.NO,
				                    null);

				dialog.set_default_response(ResponseType.NO);

				if (dialog.run () == ResponseType.YES) {
					reindex ();
				} else {
					/* Reset this suggestion */
					suggest_reindex = false;
				}

				dialog.destroy ();
			} else if (suggest_restart) {
				Dialog dialog = new MessageDialog (window,
				                                   DialogFlags.DESTROY_WITH_PARENT,
				                                   MessageType.QUESTION,
				                                   ButtonsType.NONE,
				                                   "%s\n\n%s",
				                                   _("The changes you have made to your preferences require restarting tracker processes."),
				                                   _("Would you like to restart now?"),
				                                   null);
				dialog.add_buttons (_("Restart Tracker"), ResponseType.YES,
				                    _("Do nothing"), ResponseType.NO,
				                    null);

				dialog.set_default_response(ResponseType.NO);

				if (dialog.run () == ResponseType.YES) {
					restart ();
				} else {
					/* Reset this suggestion */
					suggest_restart = false;
				}

				dialog.destroy ();
			}

			return;

		default:
			break;
		}

		Gtk.main_quit ();
	}

	[CCode (instance_pos = -1)]
	public void spinbutton_delay_value_changed_cb (SpinButton source) {
		settings_miner_fs.set_int ("initial-sleep", source.get_value_as_int ());
	}

	[CCode (instance_pos = -1)]
	public void checkbutton_enable_monitoring_toggled_cb (CheckButton source) {
		settings_miner_fs.set_boolean ("enable-monitors", source.active);
		suggest_restart = true;
	}

	[CCode (instance_pos = -1)]
	public void checkbutton_enable_index_on_battery_toggled_cb (CheckButton source) {
		settings_miner_fs.set_boolean ("index-on-battery", source.active);
		checkbutton_enable_index_on_battery_first_time.set_sensitive (!source.active);
		suggest_restart = true;
	}

	[CCode (instance_pos = -1)]
	public void checkbutton_enable_index_on_battery_first_time_toggled_cb (CheckButton source) {
		settings_miner_fs.set_boolean ("index-on-battery-first-time", source.active);
		suggest_restart = true;
	}

	[CCode (instance_pos = -1)]
	public void checkbutton_index_removable_media_toggled_cb (CheckButton source) {
		settings_miner_fs.set_boolean ("index-removable-devices", source.active);
		checkbutton_index_optical_discs.set_sensitive (source.active);
	}

	[CCode (instance_pos = -1)]
	public void checkbutton_index_optical_discs_toggled_cb (CheckButton source) {
		settings_miner_fs.set_boolean ("index-optical-discs", source.active);
	}

	[CCode (instance_pos = -1)]
	public string hscale_disk_space_limit_format_value_cb (Scale source, double value) {
		if (((int) value) == -1) {
			/* To translators: This is a feature that is
			 * disabled for disk space checking.
			 */
			return _("Disabled");
		}

		return "%d%%".printf ((int) value);
	}

	[CCode (instance_pos = -1)]
	public string hscale_drop_device_threshold_format_value_cb (Scale source, double value) {
		if (((int) value) == 0) {
			/* To translators: This is a feature that is
			 * disabled for removing a device from a
			 * database cache.
			 */
			return _("Disabled");
		}

		return "%d".printf ((int) value);
	}

	[CCode (instance_pos = -1)]
	public void button_index_add_clicked_cb (Button source) {
		store_add_dir (liststore_index);
	}

	[CCode (instance_pos = -1)]
	public void button_index_remove_clicked_cb (Button source) {
		store_del_dir (treeview_index);
	}

	[CCode (instance_pos = -1)]
	public void button_ignored_directories_globs_add_clicked_cb (Button source) {
		store_add_value_dialog (liststore_ignored_directories);
		suggest_reindex = true;
	}

	[CCode (instance_pos = -1)]
	public void button_ignored_directories_add_clicked_cb (Button source) {
		store_add_dir (liststore_ignored_directories);
		suggest_reindex = true;
	}

	[CCode (instance_pos = -1)]
	public void button_ignored_directories_remove_clicked_cb (Button source) {
		store_del_dir (treeview_ignored_directories);
		suggest_reindex = true;
	}

	[CCode (instance_pos = -1)]
	public void button_ignored_directories_with_content_add_clicked_cb (Button source) {
		store_add_value_dialog (liststore_ignored_directories_with_content);
		suggest_reindex = true;
	}

	[CCode (instance_pos = -1)]
	public void button_ignored_directories_with_content_remove_clicked_cb (Button source) {
		store_del_dir (treeview_ignored_directories_with_content);
		suggest_reindex = true;
	}

	[CCode (instance_pos = -1)]
	public void button_ignored_files_add_clicked_cb (Button source) {
		store_add_value_dialog (liststore_ignored_files);
		suggest_reindex = true;
	}

	[CCode (instance_pos = -1)]
	public void button_ignored_files_remove_clicked_cb (Button source) {
		store_del_dir (treeview_ignored_files);
		suggest_reindex = true;
	}

	private void togglebutton_directory_update_model (ToggleButton source, ListStore store, string to_check) {
		if (source.active && !model_contains (store, to_check)) {
			TreeIter iter;
			liststore_index.append (out iter);
			var v = Value (typeof (string));
			v.set_string (to_check);

			bool recurse = to_check != HOME_STRING_EVALUATED;
			liststore_index.set_value (iter, 0, v);
			liststore_index.set_value (iter, 1, recurse);
		}

		if (!source.active && model_contains (store, to_check)) {
			bool valid;
			TreeIter iter;

			valid = store.get_iter_first (out iter);
			while (valid) {
				Value value;
				store.get_value (iter, 0, out value);
				if (value.get_string () == to_check) {
					store.remove (iter);
					valid = store.get_iter_first (out iter);
				} else {
					valid = store.iter_next (ref iter);
				}
			}
		}
	}

	[CCode (instance_pos = -1)]
	public void togglebutton_home_toggled_cb (ToggleButton source) {
		togglebutton_directory_update_model (source, liststore_index, HOME_STRING_EVALUATED);
	}

	[CCode (instance_pos = -1)]
	public void togglebutton_desktop_toggled_cb (ToggleButton source) {
		togglebutton_directory_update_model (source, liststore_index, Environment.get_user_special_dir (UserDirectory.DESKTOP));
	}

	[CCode (instance_pos = -1)]
	public void togglebutton_documents_toggled_cb (ToggleButton source) {
		togglebutton_directory_update_model (source, liststore_index, Environment.get_user_special_dir (UserDirectory.DOCUMENTS));
	}

	[CCode (instance_pos = -1)]
	public void togglebutton_music_toggled_cb (ToggleButton source) {
		togglebutton_directory_update_model (source, liststore_index, Environment.get_user_special_dir (UserDirectory.MUSIC));
	}

	[CCode (instance_pos = -1)]
	public void togglebutton_pictures_toggled_cb (ToggleButton source) {
		togglebutton_directory_update_model (source, liststore_index, Environment.get_user_special_dir (UserDirectory.PICTURES));
	}

	[CCode (instance_pos = -1)]
	public void togglebutton_videos_toggled_cb (ToggleButton source) {
		togglebutton_directory_update_model (source, liststore_index, Environment.get_user_special_dir (UserDirectory.VIDEOS));
	}

	[CCode (instance_pos = -1)]
	public void togglebutton_download_toggled_cb (ToggleButton source) {
		togglebutton_directory_update_model (source, liststore_index, Environment.get_user_special_dir (UserDirectory.DOWNLOAD));
	}

	[CCode (instance_pos = -1)]
	public void checkbutton_index_file_content_toggled_cb (CheckButton source) {
		// FIXME: Should make number configurable, 10000 is the default.
		if (source.active) {
			settings_fts.reset ("max-words-to-index");
		} else {
			settings_fts.set_int ("max-words-to-index", 0);
		}

		suggest_reindex = true;
	}

	[CCode (instance_pos = -1)]
	public void checkbutton_index_numbers_toggled_cb (CheckButton source) {
		settings_fts.set_boolean ("ignore-numbers", !source.active);
		suggest_reindex = true;
	}

	[CCode (instance_pos = -1)]
	public void button_reindex_clicked_cb (Button source) {
		reindex ();
	}

	private void toggles_update (UserDirectory[] matches, bool active) {
		// Check if we need to untoggle a button
		foreach (UserDirectory ud in matches) {
			switch (ud) {
			case UserDirectory.DESKTOP:
				togglebutton_desktop.active = active;
				break;
			case UserDirectory.DOCUMENTS:
				togglebutton_documents.active = active;
				break;
			case UserDirectory.DOWNLOAD:
				togglebutton_download.active = active;
				break;
			case UserDirectory.MUSIC:
				togglebutton_music.active = active;
				break;
			case UserDirectory.PICTURES:
				togglebutton_pictures.active = active;
				break;
			case UserDirectory.VIDEOS:
				togglebutton_videos.active = active;
				break;
			}
		}
	}

	private void store_add_value_dialog (ListStore store) {
		Dialog dialog;
		Entry entry;
		Container content_area;

		dialog = new Dialog.with_buttons (_("Enter value"),
		                                  window,
		                                  DialogFlags.DESTROY_WITH_PARENT,
		                                  _("_Cancel"), ResponseType.CANCEL,
		                                  _("_OK"), ResponseType.ACCEPT);

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
				store.append (out iter);
				var v = Value (typeof (string));
				v.set_string (text);
				store.set_value (iter, 0, v);
			}
		}

		dialog.destroy ();
	}

	private void store_add_dir (ListStore store) {
		FileChooserDialog dialog = new FileChooserDialog (_("Select directory"),
		                                                  window,
		                                                  FileChooserAction.SELECT_FOLDER,
		                                                  _("_Cancel"),
		                                                  ResponseType.CANCEL,
		                                                  _("_OK"),
		                                                  ResponseType.ACCEPT);

		while (true) {
			if (dialog.run () == ResponseType.ACCEPT) {
				TreeIter iter;
				File f;

				f = dialog.get_file ();
				string path = f.get_path ();

				if (model_contains (store, path)) {
					MessageDialog md = new MessageDialog (dialog,
					                                      DialogFlags.DESTROY_WITH_PARENT,
					                                      MessageType.ERROR,
					                                      ButtonsType.CLOSE,
					                                      _("That directory is already selected as a location to index"),
					                                      null);
					md.run ();
					md.destroy ();
					continue;
				}

				string dir = f.get_path ();

				// Check which UserDirectorys we match with str
				UserDirectory[] matches = dir_match_user_directories (dir);

				// Add to store
				store.append (out iter);
				var v = Value (typeof (string));
				v.set_string (dir);
				store.set_value (iter, 0, v);

				toggles_update (matches, true);

				if (dir == HOME_STRING_EVALUATED)
					togglebutton_home.active = true;

			}

			break;
		}

		dialog.destroy ();
	}

	private void store_del_dir (TreeView view) {
		List<TreePath> list;
		ListStore store;
		TreeModel model;

		TreeSelection selection = view.get_selection ();
		list = selection.get_selected_rows (out model);

		store = (ListStore) model;

		foreach (TreePath path in list) {
			TreeIter iter;

			if (!model.get_iter (out iter, path)) {
				continue;
			}

			Value value;

			model.get_value (iter, 0, out value);
			string dir = value.get_string ();

			// Check which UserDirectorys we match with str
			UserDirectory[] matches = dir_match_user_directories (dir);

			store.remove (iter);

			// Check if we need to untoggle a button
			toggles_update (matches, false);

			if (dir == HOME_STRING_EVALUATED)
				togglebutton_home.active = false;
		}
	}

	private UserDirectory[] dir_match_user_directories (string input) {
		UserDirectory[] matches = {};
		int i;

		for (i = 0; i < UserDirectory.N_DIRECTORIES; i++) {
			UserDirectory ud = (UserDirectory) i;
			unowned string dir = null;

			dir = Environment.get_user_special_dir (ud);
			if (input == dir) {
				matches += ud;
			}
		}

		return matches;
	}

	private string dir_to_config (string input) {
		string output = input;

		if (HOME_STRING_EVALUATED != null && HOME_STRING_EVALUATED == input) {
			return HOME_STRING;
		}

		for (int i = 0; i < UserDirectory.N_DIRECTORIES; i++) {
			UserDirectory ud = (UserDirectory) i;
			unowned string dir = null;

			dir = Environment.get_user_special_dir (ud);
			if (input == dir) {
				// Convert 'G_USER_DIRECTORY_FOO' to '&FOO'
				string ud_string = ud.to_string ();
				output = "&%s".printf (ud_string.substring (ud_string.last_index_of_char ('_') + 1, -1));
			}
		}

		return output;
	}

	private string dir_from_config (string input) {
		string output = input;

		if (input.has_prefix ("&")) {
			unowned string dir = null;

			// Convert '&FOO' to 'G_USER_DIRECTORY_FOO'
			string ud_input = "G_USER_DIRECTORY_%s".printf (input.next_char ());

			for (int i = 0; i < UserDirectory.N_DIRECTORIES && dir == null; i++) {
				UserDirectory ud = (UserDirectory) i;

				if (ud_input == ud.to_string ()) {
					dir = Environment.get_user_special_dir (ud);
				}
			}

			// debug ("Found dir '%s' evaluates to '%s'", input, dir);

			if (dir != null)
				output = dir;
		} else if (input.has_prefix ("$")) {
			unowned string env = Environment.get_variable (input.substring (1, -1));

			// debug ("Found env '%s' (%s) evaluates to '%s'", input, input.substring (1, -1), env);

			if (env != null)
				output = env;
		}

		return output;
	}

	private string[] model_to_strv (ListStore model, bool recurse_required, bool recurse_value) {
		string[] list = {};
		TreeIter iter;
		bool valid;

		for (valid = model.get_iter_first (out iter);
		     valid;
		     valid = model.iter_next (ref iter)) {
			Value value;

			model.get_value (iter, 0, out value);

			if (recurse_required) {
				Value recurse;

				model.get_value (iter, 1, out recurse);

				if (recurse_value != recurse.get_boolean ())
					continue;
			}

			// Convert from real value to config values,
			// e.g. '$HOME/Desktop' to '&DESKTOP'
			string dir = dir_to_config (value.get_string ());
			list += dir;
		}

		return list;
	}

	public bool model_contains (TreeModel model, string needle) {
		TreeIter iter;
		string needle_evaluated;
		bool valid;

		needle_evaluated = dir_from_config (needle);

		for (valid = model.get_iter_first (out iter);
		     valid;
		     valid = model.iter_next (ref iter)) {
			Value value;

			model.get_value (iter, 0, out value);

			if (value.get_string () == needle_evaluated) {
				return true;
			}
		}

		return false;
	}

	private void model_populate (ListStore model, string[] list, bool have_recurse, bool recurse) {
		int position = 0;

		foreach (string s in list) {
			// Convert any dirs from config to real values
			bool ignore = false;

			// Don't insert configs if toggle is not sensitive
			foreach (UserDirectory ud in ignored_user_directories) {
				string ud_string = ud.to_string ();
				string output = "&%s".printf (ud_string.substring (ud_string.last_index_of_char ('_') + 1, -1));

				if (s == output) {
					ignore = true;
					break;
				}
			}

			if (ignore) {
				debug ("Ignoring '%s' (duplicates other entries in config)", s);
				continue;
			}

			string str = dir_from_config (s);

			try {
				if (have_recurse)
					model.insert_with_values (null,
					                          position++,
					                          0,
					                          Filename.to_utf8 (str,
					                                            -1,
					                                            null,
					                                            null),
					                          1,
					                          recurse,
					                          -1);
				else
					model.insert_with_values (null,
					                          position++,
					                          0,
					                          Filename.to_utf8 (str,
					                                            -1,
					                                            null,
					                                            null),
					                          -1);
			} catch (GLib.ConvertError e) {
				print ("Could not convert filename to UTF8: %s", e.message);
			}
		}
	}

	private void treeview_setup (TreeView view, string title, bool show_recurse_column, bool sort) {
		TreeViewColumn column;
		GLib.List<weak TreeViewColumn> columns = view.get_columns ();

		// Needed to fix glade mess
		foreach (TreeViewColumn c in columns) {
			view.remove_column (c);
		}

		column = new TreeViewColumn.with_attributes (title,
		                                             new CellRendererText (),
		                                             "text", 0,
		                                             null);
		column.set_expand (true);
		view.append_column (column);

		if (show_recurse_column) {
			ListStore store = view.get_model () as ListStore;
			CellRendererToggle cell = new CellRendererToggle ();

			column = new TreeViewColumn.with_attributes (_("Recurse"),
			                                             cell,
			                                             "active", 1,
			                                             null);
			column.set_expand (false);
			column.set_fixed_width (50);
			view.append_column (column);

			cell.toggled.connect ((toggle, path) => {
				var tree_path = new TreePath.from_string (path);
				TreeIter iter;

				store.get_iter (out iter, tree_path);
				store.set (iter, 1, !toggle.active);
			});
		}

		if (sort) {
			TreeSortable sortable = view.get_model() as TreeSortable;
			sortable.set_sort_column_id (0, SortType.ASCENDING);
		}
	}

	private void sanitize_user_dirs () {
		string[] all_dirs = null;

		all_dirs += HOME_STRING_EVALUATED;

		for (int i = 0; i < UserDirectory.N_DIRECTORIES; i++) {
			UserDirectory ud = (UserDirectory) i;

			string dir = Environment.get_user_special_dir (ud);
			if (dir == null) {
				continue;
			}

			foreach (string d in all_dirs) {
				if (d == dir) {
					debug ("Directory '%s' duplicated in XDG dir %d", d, ud);

					switch (ud) {
					case UserDirectory.DESKTOP:
					case UserDirectory.DOCUMENTS:
					case UserDirectory.DOWNLOAD:
					case UserDirectory.MUSIC:
					case UserDirectory.PICTURES:
					case UserDirectory.VIDEOS:
						ignored_user_directories += ud;
						break;

					default:
						// We don't care about others, we don't
						// have toggle buttons for them
						break;
					}

					break;
				}
			}

			// Add dir to list of dirs we know about to filter
			// out subsequent dirs
			all_dirs += dir;
		}

		foreach (UserDirectory ud in ignored_user_directories) {
			switch (ud) {
			case UserDirectory.DESKTOP:
				togglebutton_desktop.sensitive = false;
				break;
			case UserDirectory.DOCUMENTS:
				togglebutton_documents.sensitive = false;
				break;
			case UserDirectory.DOWNLOAD:
				togglebutton_download.sensitive = false;
				break;
			case UserDirectory.MUSIC:
				togglebutton_music.sensitive = false;
				break;
			case UserDirectory.PICTURES:
				togglebutton_pictures.sensitive = false;
				break;
			case UserDirectory.VIDEOS:
				togglebutton_videos.sensitive = false;
				break;
			default:
				break;
			}
		}
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
