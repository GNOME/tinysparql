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


[DBus (name = "org.freedesktop.Tracker1.Resources")]
interface Resources : GLib.Object
    def abstract SparqlQuery (query : string) : array of string[,]  raises DBus.Error
    def abstract SparqlUpdate (query : string) raises DBus.Error


class TrackerQuery : Object

    tracker : Resources
    _SearchTerms : string

    event SearchSettingsChanged ()
    event ClearSearchResults ()

    prop SearchTerms : string
        get
            return  _SearchTerms
        set
            if value is not null
                _SearchTerms = value

    prop Category : string
    prop SortField : string
    prop Fields : array of string


    init
        Category = "All"

        notify += def (t, propety)
            if propety.name is "Category" or  propety.name is "SortField" or propety.name is "Fields"
                SearchSettingsChanged ()
            else
                if propety.name is "SearchTerms"
                    if SearchTerms is null or SearchTerms.length < 3
                        ClearSearchResults ()
                    else
                        SearchSettingsChanged ()

    def Connect () : bool

        try
            var conn = DBus.Bus.get (DBus.BusType.SESSION)
            tracker = conn.get_object ("org.freedesktop.Tracker1", "/org/freedesktop/Tracker1/Resources", "org.freedesktop.Tracker1.Resources") as Resources
        except e : DBus.Error
            print "Cannot connect to Session bus. Error is %s", e.message
            return false

        return true


    def Search () : array of string[,]?

        cat, query : string

        if Category is null or Category is "All"
            cat = "nfo:FileDataObject"
        else
            cat = Category

        query = "SELECT ?s nie:url(?s) nie:mimeType(?s) WHERE { ?s fts:match \"%s\". ?s a %s } LIMIT 100 ".printf (SearchTerms, cat)

        // to do : add Fields, Category and SortField
        try
            return tracker.SparqlQuery (query)
        except e:DBus.Error
            print "Dbus error : %s", e.message

        return null


    def Query (sparql : string) : array of string[,]?
        try
            return tracker.SparqlQuery (sparql)
        except e:DBus.Error
            print "Dbus error : %s", e.message

        return null
