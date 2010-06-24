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


enum CategoryColumns
    Icon
    Name
    DisplayName
    NumOfCols


const icon_size : int = 16

class TrackerCategoryView : ScrolledWindow
    store : ListStore
    treeview : TreeView
    lab : Label
    catergory : Categories

    prop Query : TrackerQuery

    init

        hscrollbar_policy = PolicyType.NEVER
        vscrollbar_policy = PolicyType.AUTOMATIC
        shadow_type = ShadowType.ETCHED_OUT

        store = new ListStore (CategoryColumns.NumOfCols, typeof (Gdk.Pixbuf), typeof (string), typeof (string))
        iter : TreeIter

        store.append (out iter);
        store.set (iter, CategoryColumns.Icon, GetThemePixbufByName ("system-file-manager", icon_size, get_screen ()), \
                   CategoryColumns.Name, "All", CategoryColumns.DisplayName, N_("All Files") , -1);

        store.append (out iter);
        store.set (iter, CategoryColumns.Icon, GetThemePixbufByName ("folder", icon_size, get_screen ()), \
                   CategoryColumns.Name, "nfo:Folder", CategoryColumns.DisplayName, N_("Folders") , -1);

        store.append (out iter);
        store.set (iter, CategoryColumns.Icon, GetThemePixbufByName ("x-office-document", icon_size, get_screen ()), \
                   CategoryColumns.Name, "nfo:Document", CategoryColumns.DisplayName, N_("Documents") , -1);

        store.append (out iter);
        store.set (iter, CategoryColumns.Icon, GetThemePixbufByName ("image-x-generic", icon_size, get_screen ()), \
                   CategoryColumns.Name, "nfo:Image", CategoryColumns.DisplayName, N_("Images") , -1);

        store.append (out iter);
        store.set (iter, CategoryColumns.Icon, GetThemePixbufByName ("audio-x-generic", icon_size, get_screen ()), \
                   CategoryColumns.Name, "nmm:MusicPiece", CategoryColumns.DisplayName, N_("Music") , -1);

        store.append (out iter);
        store.set (iter, CategoryColumns.Icon, GetThemePixbufByName ("video-x-generic", icon_size, get_screen ()), \
                   CategoryColumns.Name, "nmm:Video", CategoryColumns.DisplayName, N_("Videos") , -1);

        store.append (out iter);
        store.set (iter, CategoryColumns.Icon, GetThemePixbufByName ("system-run", icon_size, get_screen ()), \
                   CategoryColumns.Name, "nfo:SoftwareApplication", CategoryColumns.DisplayName, N_("Applications") , -1);

        store.append (out iter);
        store.set (iter, CategoryColumns.Icon, GetThemePixbufByName ("evolution-mail", icon_size, get_screen ()), \
                   CategoryColumns.Name, "nmo:Email", CategoryColumns.DisplayName, N_("Emails") , -1);

        treeview = new TreeView.with_model (store)
        treeview.insert_column_with_attributes (-1, "icon", new CellRendererPixbuf (), "pixbuf", 0, null)
        treeview.insert_column_with_attributes (-1, "name", new CellRendererText (), "text", 2, null)
        treeview.set_headers_visible (false)
        treeview.set_enable_search (false)

        var category_selection = treeview.get_selection ()
        category_selection.set_mode (SelectionMode.BROWSE)

        category_selection.changed += selection_changed

        add (treeview)

        show_all ()

    def selection_changed (sel : TreeSelection)
        iter : TreeIter
        model : TreeModel
        name : weak string

        sel.get_selected (out model, out iter)
        store.get (iter, CategoryColumns.Name, out name)

        if Query is not null
            Query.Category = name
