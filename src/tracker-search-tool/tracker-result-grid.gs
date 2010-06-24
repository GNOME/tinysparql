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


enum ResultColumns
    Id
    Uri
    Icon
    DisplayName
    Mime
    Category
    Snippet
    IsDirectory
    Path
    NumOfCols


const targets : array of TargetEntry[] = {{ "text/uri-list", 0, 1 },{ "text/plain",    0, 0 },{ "STRING",	   0, 0 }}


class TrackerResultGrid : ScrolledWindow
    store : ListStore
    iconview: IconView
    _query : TrackerQuery

    event SelectionChanged (path : TreePath?)

    prop Query : TrackerQuery
        get
            return _query
        set
            if value is not null
                _query = value
                _query.SearchSettingsChanged += def ()
                    RefreshQuery ()
                _query.ClearSearchResults += def ()
                    store.clear ()


    def GetSelectedPath () : TreePath?
        if iconview.get_selected_items () is not null
            return (Gtk.TreePath) iconview.get_selected_items ().data

        return null


    def GetSelectedUri () : weak string
        iter : TreeIter
        uri : weak string
        var path = GetSelectedPath ()

        if path is not null
            store.get_iter (out iter, path)
            store.get (iter, ResultColumns.Uri, out uri);
            return uri

        return ""


    init

        hscrollbar_policy = PolicyType.AUTOMATIC
        vscrollbar_policy = PolicyType.AUTOMATIC
        shadow_type = ShadowType.ETCHED_OUT

        store = new ListStore (ResultColumns.NumOfCols, typeof (string), typeof (string), typeof (Gdk.Pixbuf), typeof (string), \
                               typeof (string), typeof (string), typeof (string), typeof (bool), typeof (string))

        // to do add treeview

        iconview = new IconView.with_model (store)
        iconview.set_pixbuf_column (ResultColumns.Icon)
        iconview.set_text_column (ResultColumns.DisplayName)
        iconview.set_selection_mode (SelectionMode.BROWSE)
        iconview.enable_model_drag_source (Gdk.ModifierType.BUTTON1_MASK | Gdk.ModifierType.BUTTON2_MASK, targets, Gdk.DragAction.COPY | Gdk.DragAction.MOVE | Gdk.DragAction.ASK)
        iconview.set_item_width (150)
        iconview.set_row_spacing (10)
        iconview.item_activated += ActivateUri

        iconview.selection_changed += def ()
            var path = GetSelectedPath ()
            SelectionChanged (path)

        /* set correct uri for drag drop  */
        iconview.drag_data_get +=  def (context, data, info, time)
            var uri = GetSelectedUri ()
            if uri is not null
                var s = new array of string [1]
                s[0] = uri
                data.set_uris (s)

        add (iconview)
        show_all ()


    def RefreshQuery ()
        if _query is not null
            var results = _query.Search ()
            var has_results = false
            iter : TreeIter

            store.clear ()

            if results is null do return
            
            var i = 0
            while results[i] is not null
                var uri = results[i+1]
                var id = results[i]
                var mime = results[i+2]                     
                i += 3

                if uri.has_prefix ("file://")

                    has_results = true
                    var handled = false

                    var file = File.new_for_uri (uri)

                    var query = "SELECT rdf:type(?s) where { ?s nie:url \"%s\" }".printf(uri)
                    var qresults = Query.Query (query)

                    if qresults is not null and qresults[0].contains ("nfo#Software")
                        app_info : AppInfo
                        app_info = new DesktopAppInfo.from_filename (file.get_path ())

                        if app_info is not null
                            store.append (out iter);
                            store.set (iter, ResultColumns.Id, id, ResultColumns.Uri, uri, ResultColumns.Mime, mime, ResultColumns.Icon, GetThemeIconPixbuf (app_info.get_icon (), 48, get_screen()), \
                                      ResultColumns.DisplayName, app_info.get_display_name(), ResultColumns.IsDirectory, \
                                      false , -1)
                            handled = true

                    if not handled
                        try
                            var info =  file.query_info ("standard::display-name,standard::icon,thumbnail::path", \
                                                         FileQueryInfoFlags.NONE, null)

                            var filetype =  info.get_file_type ()
                            store.append (out iter);
                            store.set (iter, ResultColumns.Id, id, ResultColumns.Uri, uri, ResultColumns.Mime, mime, ResultColumns.Icon, GetThumbNail (info, 64, 48, get_screen()), \
                                      ResultColumns.DisplayName, info.get_display_name(), ResultColumns.IsDirectory, \
                                      (filetype is FileType.DIRECTORY) , -1)
                                      
                        except e:Error
                            print "Could not get file info for %s", uri

                else
                    if uri.has_prefix ("email://")

                        var query = "SELECT nmo:messageSubject(?s) where { ?s nie:url \"%s\" }".printf(uri)
                        var qresults = Query.Query (query)

                        store.append (out iter);
                        store.set (iter, ResultColumns.Id, id, ResultColumns.Uri, uri, ResultColumns.Mime, mime, ResultColumns.Icon, GetThemePixbufByName ("evolution-mail", 48, get_screen ()), \
                                   ResultColumns.DisplayName, qresults[0], ResultColumns.IsDirectory, false, -1)


            /* select first result */
            if has_results
                var path = new TreePath.from_string ("0:0:0")
                if path is not null
                    iconview.select_path (path)


    def ActivateUri (path : TreePath)
        iter : TreeIter
        is_dir : bool = false

        store.get_iter (out iter, path)
        uri : weak string
        store.get (iter, ResultColumns.Uri, out uri);
        store.get (iter, ResultColumns.IsDirectory, out is_dir);

        var query = "SELECT rdf:type(?s) where { ?s nie:url \"%s\" }".printf(uri)
        var results = Query.Query (query)
        if results is not null and results[0].contains ("nfo#Software")
            LaunchApp (uri)
        else
            OpenUri (uri, is_dir)
