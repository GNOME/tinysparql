[indent=4]

/*
 * Copyright (C) 2009, Jamie McCracken (jamiecc at gnome org)
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
    
    
const static RUN_DELAY : int = 500
    
class TrackerSearchEntry  : Gtk.Entry implements Gtk.Activatable
    id_invoker : uint = 0

    prop Query : TrackerQuery

    init
        set_icon_from_stock (EntryIconPosition.SECONDARY, STOCK_CLEAR)
        set_icon_sensitive (EntryIconPosition.PRIMARY, false)
        set_icon_sensitive (EntryIconPosition.SECONDARY, false)
        set_icon_tooltip_text (EntryIconPosition.SECONDARY, _("Clear the search text"))
        activate += entry_activate
        changed += entry_changed
        icon_press += def (p0, p1)
            if p0 is EntryIconPosition.SECONDARY
                text = "" 
        
        
    def private entry_changed (editable : Editable) 
        if Query is not null
            if text is null
                Query.SearchTerms = ""
                if id_invoker != 0
                    Source.remove (id_invoker)
                    id_invoker = 0
                set_icon_sensitive (EntryIconPosition.SECONDARY, false)
            else
                if id_invoker != 0
                    Source.remove (id_invoker)
                id_invoker = Timeout.add (RUN_DELAY, run_query)

    def private entry_activate (entry : TrackerSearchEntry)
        entry.grab_focus ()

    def private run_query () : bool
        if Query is not null
            if (text is null) or (text is "")
                set_icon_sensitive (EntryIconPosition.SECONDARY, false)
                Query.SearchTerms = ""
            else
                set_icon_sensitive (EntryIconPosition.SECONDARY, true)
                Query.SearchTerms = EscapeSparql (text, true)
        return false

    def sync_action_properties (action : Action)
        return

    def update (action : Action, prop : string)
        return
