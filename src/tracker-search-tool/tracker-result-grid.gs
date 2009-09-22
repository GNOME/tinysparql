[indent=4]

uses
    Gtk
    TrackerUtils
    
    
enum ResultColumns
    Uri
    Icon
    DisplayName
    Mime
    Category
    Snippet
    IsDirectory
    NumOfCols


const targets : array of TargetEntry[] = {{ "text/uri-list", 0, 1 },{ "text/plain",    0, 0 },{ "STRING",	   0, 0 }}

    
class TrackerResultGrid : ScrolledWindow
    store : ListStore
    iconview: IconView
    _query : TrackerQuery
    
    event SelectedUriChanged ()    
    
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
                        

    init
    
        hscrollbar_policy = PolicyType.NEVER
        vscrollbar_policy = PolicyType.AUTOMATIC
        shadow_type = ShadowType.ETCHED_OUT
           
        store = new ListStore (ResultColumns.NumOfCols, typeof (string), typeof (Gdk.Pixbuf), typeof (string), \
                               typeof (string), typeof (string), typeof (string), typeof (bool))

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
            SelectedUriChanged ()

        /* set correct uri for drag drop  */
        iconview.drag_data_get +=  def (context, data, info, time)
            l :  weak GLib.List of TreePath
            l = iconview.get_selected_items ()
            if l is not null and l.data is not null
                iter : TreeIter
                uri : weak string
                path : TreePath = l.data
                store.get_iter (out iter, path)
                store.get (iter, ResultColumns.Uri, out uri);
                var s = new array of string [1]
                s[0] = uri
                data.set_uris (s) 
          
        
      
		
        add (iconview)
		
        show_all ()

                
    def RefreshQuery ()
        if _query is not null
            var results = _query.Search ()
            iter : TreeIter

            store.clear ()
            
            for uri in results
                if uri.has_prefix ("file://")
                    
                    var file = File.new_for_uri (uri)
                    
                    try
                        var info =  file.query_info ("standard::display-name,standard::icon,thumbnail::path", \
                                                     FileQueryInfoFlags.NONE, null) 
                                                 
                        var filetype =  info.get_file_type ()
                        store.append (out iter);
                        store.set (iter, ResultColumns.Uri, uri, ResultColumns.Icon, GetThumbNail (info, 64, 48, get_screen()), \
                                  ResultColumns.DisplayName, info.get_display_name(), ResultColumns.IsDirectory, \
                                  (filetype is FileType.DIRECTORY) , -1);
                    except e:Error
                        print "Could not get file info for %s", uri
            
                    
    
     
    def ActivateUri (path : TreePath)
        iter : TreeIter
        is_dir : bool = false
        
        store.get_iter (out iter, path)
        uri : weak string
        store.get (iter, ResultColumns.Uri, out uri);
        store.get (iter, ResultColumns.IsDirectory, out is_dir);
        
        OpenUri (uri, is_dir)

            
    
		

    
