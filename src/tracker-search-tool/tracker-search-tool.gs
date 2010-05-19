[indent=4]

/*
 * Copyright (C) 2009 Mr Jamie McCracken (jamiemcc at gnome org)
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

uses
    Gtk

[CCode (cheader_filename = "config.h")]
const extern static PACKAGE_VERSION : string

const static ABOUT : string = "Tracker " + PACKAGE_VERSION + "\n"

const static LICENSE : string =\
"This program is free software and comes without any warranty.\n" +\
"It is licensed under version 2 or later of the General Public "  +\
"License which can be viewed at:\n"                               +\
"\n"                                                              +\
"  http://www.gnu.org/licenses/gpl.txt\n"

window : Window
terms : array of string?
search_string : string?
print_version: bool
const options : array of OptionEntry = { {"version", 'V', 0, OptionArg.NONE, ref print_version, "Print version", null }, {"", 0, 0, OptionArg.STRING_ARRAY, ref terms, "search terms", null}, { null }}

[DBus (name = "org.freedesktop.Tracker1.SearchTool")]
class TrackerSearchToolServer : GLib.Object
    def Show ()
        window.present ()

init
    Gtk.init (ref args)

    /* get options */

    var option_context = new OptionContext ("tracker-search-tool")
    option_context.set_help_enabled (true)
    option_context.add_main_entries (options, null)

    try
        option_context.parse (ref args)

    except e : GLib.OptionError

        stdout.printf ("%s\n", e.message)
        stdout.printf ("Run '%s --help' to see a full list of available command line options.\n", args[0])
        return

    if (print_version)
        stdout.printf ("\n" + ABOUT + "\n" + LICENSE + "\n");
        return

    var server = new TrackerSearchToolServer

    try
        bus : dynamic DBus.Object
        result : uint

        var conn = DBus.Bus.get (DBus.BusType.SESSION)

        bus = conn.get_object ("org.freedesktop.DBus", \
                               "/org/freedesktop/DBus", \
                               "org.freedesktop.DBus")

        result = bus.request_name ("org.freedesktop.Tracker1.SearchTool", (uint) 0)

        if (result is DBus.RequestNameReply.PRIMARY_OWNER)
            conn.register_object ("/org/freedesktop/Tracker1/SearchTool", server)
        else
            /* There's another instance, pop it up */
            remote : dynamic DBus.Object
            remote = conn.get_object ("org.freedesktop.Tracker1.SearchTool", \
                                      "/org/freedesktop/Tracker1/SearchTool", \
                                      "org.freedesktop.Tracker1.SearchTool")

            remote.show ()
            return
    except e : DBus.Error
        warning ("%s", e.message)

    var builder = new Builder ()

    try
        builder.add_from_file (SRCDIR + "tst.ui")

    except e : GLib.Error

        try
            builder.add_from_file (UIDIR + "tst.ui")
        except e : GLib.Error
            var msg = new MessageDialog (null, DialogFlags.MODAL, \
                                         MessageType.ERROR, ButtonsType.OK, \
                                        N_("Failed to load UI\n%s"), e.message)
            msg.run ()
            Gtk.main_quit()

    window = builder.get_object ("window") as Window
    window.destroy += Gtk.main_quit

    /* create widgets */
    var
        accel_group = new AccelGroup ()
        entry = new Entry ()

        query = new TrackerQuery
        search_entry = new TrackerSearchEntry ()
        grid = new TrackerResultGrid ()
        categories = new TrackerCategoryView ()
        tile = new TrackerMetadataTile ()

        entry_box = builder.get_object ("EntryBox") as Container
        grid_box = builder.get_object ("GridBox") as Container
        category_box = builder.get_object ("CategoryBox") as Container
        main_box = builder.get_object ("MainBox") as VBox
        search_label = builder.get_object ("SearchLabel") as Label

    window.add_accel_group (accel_group)
    search_label.set_mnemonic_widget (search_entry)

    query.Connect ()
    search_entry.Query = query
    entry_box.add (search_entry)

    keyval : uint
    mods : Gdk.ModifierType
    accelerator_parse ("<Ctrl>s", out keyval, out mods)
    entry = search_entry.get_child () as Entry
    entry.add_accelerator ("activate", accel_group, keyval, mods,
                            AccelFlags.VISIBLE | AccelFlags.LOCKED)

    grid.Query = query
    grid_box.add (grid)

    categories.Query = query
    category_box.add (categories)

    tile.ResultGrid = grid
    tile.Query = query

    var a = new Alignment (0.5f, 0.5f, 1.0f, 0.5f)   
    a.add (tile)
    main_box.pack_end (a, false, false, 0)

    window.show_all ()

    if (terms is not null)
        search_string = string.joinv (" ", terms)
        entry.set_text (search_string)

    Gtk.main ()
