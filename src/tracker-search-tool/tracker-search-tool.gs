[indent=4]

/*
 * Copyright (C) 2009 Mr Jamie McCracken (jamiecc at gnome org)
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




