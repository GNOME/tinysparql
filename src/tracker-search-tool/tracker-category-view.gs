[indent=4]

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
        store.set (iter, CategoryColumns.Icon, GetThemePixbufByName ("x-office-document", icon_size, get_screen ()), \
                   CategoryColumns.Name, "nfo:Document", CategoryColumns.DisplayName, N_("Office Documents") , -1);
                   
        store.append (out iter);
        store.set (iter, CategoryColumns.Icon, GetThemePixbufByName ("text-x-generic", icon_size, get_screen ()), \
                   CategoryColumns.Name, "nfo:TextDocument", CategoryColumns.DisplayName, N_("Text Documents") , -1);
                   
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
                
        treeview = new TreeView.with_model (store)
        treeview.insert_column_with_attributes (-1, "icon", new CellRendererPixbuf (), "pixbuf", 0, null)
        treeview.insert_column_with_attributes (-1, "name", new CellRendererText (), "text", 2, null)
        treeview.set_headers_visible (false)
        
        var category_selection = treeview.get_selection ()
        category_selection.set_mode (SelectionMode.BROWSE)
        
        category_selection.changed += selection_changed 
        
        add (treeview)
        
        show_all ()

    def selection_changed (sel : TreeSelection)
        iter : TreeIter
        model : TreeModel
        
        sel.get_selected (out model , out iter)
        
        name : weak string
        
        store.get (iter, CategoryColumns.Name, out name);
        
        if Query is not null
            Query.Category = name

            
   

		

    
