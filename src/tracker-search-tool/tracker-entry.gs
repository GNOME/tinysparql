[indent=4]

/*
 * Copyright (C) 2009, Jamie McCracken (jamiemcc at gnome org)
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


uses
    Gtk
    TrackerUtils

/* wait 0.5s after each keystroke before performing a search */
const static RUN_DELAY : int = 500

/* acceptable length of string for history */
const static TOO_SHORT : int = 2

class TrackerSearchEntry  : ComboBoxEntry implements Gtk.Activatable
    id_invoker : uint = 0
    entry : Entry
    history : list of string
    histfilename : string
    histfile : FileStream

    prop Query : TrackerQuery

    init
        entry = get_child() as Entry
        entry.set_icon_from_stock (EntryIconPosition.SECONDARY, STOCK_CLEAR)
        entry.set_icon_sensitive (EntryIconPosition.SECONDARY, true)
        entry.set_icon_tooltip_text (EntryIconPosition.SECONDARY,
                                     _("Clear the search text"))

        var model = new ListStore (1, typeof (string))
        set_model(model)
        set_text_column(0)

        var completion = new EntryCompletion ()
        completion.set_model(model)
        completion.set_text_column(0)
        entry.set_completion(completion)

        entry.activate += entry_activate
        entry.changed += entry_changed
        entry.icon_press += def (p0)
            if p0 is EntryIconPosition.SECONDARY
                entry.text = ""

        histfilename = Path.build_filename (Environment.get_user_data_dir(),
            "tracker", "history", null)
        var temp = ""
        history = new list of string

        /* load history from file */
        try
            FileUtils.get_contents(histfilename, out temp)
            for item in temp.split("\n")
                if item.len() > TOO_SHORT
                    prepend_text(item)
                    history.add(item)
        except e : FileError
            print (e.message)

    def private entry_changed ()
        if entry.text is null
            Query.SearchTerms = ""
            if id_invoker != 0
                Source.remove (id_invoker)
                id_invoker = 0
        else
            if id_invoker != 0
                Source.remove (id_invoker)
            id_invoker = Timeout.add (RUN_DELAY, run_query)

    def private entry_activate ()
        entry.grab_focus ()

    def private run_query () : bool
        var txt = entry.text
        if (txt is null) or (txt is "")
            Query.SearchTerms = ""
            return false

        Query.SearchTerms = EscapeSparql (txt, true)

        history_handler(txt)
        return false

    def history_handler (text : string)
        /* remove leading and trailing whitespace before inserting items into
         * history; this avoids having both "term" and " term " in there;
         */
        var txt = text.strip()

        /* ensure that accented chars are represented the same way to
         * avoid duplicated entries in history
         */
        txt = txt.normalize(-1, NormalizeMode.NFC)

        if txt.len() > TOO_SHORT

            /* do not store duplicate items in history */
            for item in history
                if txt == item
                    return

            history.add(txt)
            prepend_text(txt)
            histfile = FileStream.open(histfilename, "a")
            if (FileStream.open(histfilename, "a") != null)
                histfile.printf("%s\n", txt)
            else
                print("error: '%s' is not writable", histfilename)

    def sync_action_properties (action : Action)
        return

    def update (action : Action, prop : string)
        return
