[indent=4]

uses
    Gtk


init  
    Gtk.init (ref args)
    
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
    
              
    var window = builder.get_object ("window") as Window
    window.destroy += Gtk.main_quit
        
    /* create tracker widgets */    
        
    var 
        query = new TrackerQuery
        entry = new TrackerSearchEntry ()
        grid = new TrackerResultGrid ()
        categories = new TrackerCategoryView ()
        tile = new TrackerMetadataTile ()
        
        entry_box = builder.get_object ("EntryBox") as Container
        grid_box = builder.get_object ("GridBox") as Container
        category_box = builder.get_object ("CategoryBox") as Container
        detail_box = builder.get_object ("DetailBox") as Container
        
        
    query.Connect ()    
    entry.Query = query 
    entry_box.add (entry)
    
    grid.Query = query
    grid_box.add (grid)
    
    categories.Query = query
    category_box.add (categories)
    
    tile.ResultGrid = grid
    tile.Query = query
    detail_box.add (tile)

    window.show_all ()
        
    Gtk.main ()




