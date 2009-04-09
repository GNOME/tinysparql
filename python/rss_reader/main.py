#!/usr/bin/env python2.5
#
# Demo RSS client using tracker as backend
# Copyright (C) 2009 Nokia <urho.konttori@nokia.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
# USA
#
import gtk
import gobject
import pango
import dbus
from dbus.mainloop.glib import DBusGMainLoop
from tracker_backend import TrackerRSS

# uri, title, date, isread, pango.Weight
posts_model = gtk.ListStore (str, str, str, bool, int)

# uri, title, entries, enabled
channels_model = gtk.ListStore (str, str, str, bool)

posts_treeview = None
channels_treeview = None

sources_dialog = None

# Convenience defines
FALSE = "false"
TRUE = "true"
NORMAL = pango.WEIGHT_NORMAL
BOLD = pango.WEIGHT_BOLD

tracker = TrackerRSS ()

def populate_initial_posts ():
    results = tracker.get_post_sorted_by_date (10)
    
    for result in results:
        if (result[3] == FALSE):
            row = (result[0], result[1], result[2], False, BOLD)
        else:
            row = (result[0], result[1], result[2], True, NORMAL)
            
        posts_model.append (row)

def create_posts_tree_view ():
    treeview = gtk.TreeView ()
    column_title = gtk.TreeViewColumn ("Title",
                                       gtk.CellRendererText (),
                                       text=1, weight=4)
    column_date = gtk.TreeViewColumn ("Date",
                                       gtk.CellRendererText (),
                                       text=2)

    treeview.append_column (column_title)
    treeview.append_column (column_date)

    return treeview

def create_channels_tree_view ():
    treeview = gtk.TreeView ()
    toggle = gtk.CellRendererToggle ()
    toggle.set_property ("activatable", True)
    def toggled (widget, path):
        it = channels_model.get_iter_from_string (path)
        uri, old_value = channels_model.get (it, 0, 3)
        if (old_value == True):
            tracker.mark_as_invisible (uri)
        else:
            tracker.mark_as_visible (uri)
        channels_model.set (it, 3, not old_value)
    toggle.connect ("toggled",
                    toggled)
    column_enable = gtk.TreeViewColumn ("Enabled",
                                        toggle,
                                        active=3)
    column_title = gtk.TreeViewColumn ("Title",
                                       gtk.CellRendererText (),
                                       text=1)
    column_entries = gtk.TreeViewColumn ("Entries",
                                         gtk.CellRendererText (),
                                         text=2)
    treeview.append_column (column_enable)
    treeview.append_column (column_title)
    treeview.append_column (column_entries)
    return treeview
    
def toggle_row_foreach (treemodel, path, iter):
    uri, readed, text = treemodel.get (iter, 0, 3 ,1)
    if (readed):
        #Mark as unread
        treemodel.set_value (iter, 3, False)
        treemodel.set_value (iter, 4, BOLD)
        tracker.set_is_read (uri, False)
    else:
        #Mark as readed
        treemodel.set_value (iter, 3, True)
        treemodel.set_value (iter, 4, NORMAL)
        tracker.set_is_read (uri, True)

def clicked_toggle_cb (widget):
    selected = posts_treeview.get_selection ()
    if (selected.count_selected_rows () == 0):
        return

    selected.selected_foreach (toggle_row_foreach)

def clicked_sources_cb (widget, dialog):
    # Dont do this all the time!
    if (len (channels_model) == 0):
        feeds = tracker.get_all_subscribed_feeds ()
        for (uri, name, value, visible) in feeds:
            channels_model.append ((uri, name, value, visible))

    channels_treeview.show_all ()
    retval = dialog.run ()
    if (retval == gtk.RESPONSE_ACCEPT):
        pass
    
    dialog.hide ()

def notification_addition (added_uris):
    print "Received signal"
    print "%d add: %s" % (len(added_uris), [str(n) for n in added_uris])
    for uri in added_uris:
        details = tracker.get_info_for_entry (uri)
        if (details):
            if (details[2]):
                model_row = (uri, details[0], details[1],
                             details[2], NORMAL)
            else:
                model_row = (uri, details[0], details[1],
                             details[2], BOLD)
            posts_model.prepend (model_row)

def remove_uris (model, path, iter, user_data):
    uri = model.get (iter, 0)
    if (uri[0] in user_data):
        model.remove (iter)
        return True

def notification_removal (removed_uris):
    print "Received signal"
    print "%d remove: %s" % (len(removed_uris), [str(n) for n in removed_uris])
    posts_model.foreach (remove_uris, removed_uris)

def update_uri (model, path, iter, user_data):
    updated_uri = user_data [0]
    uri = model.get (iter, 0)
    print "Comparing ", uri[0], "with", updated_uri
    if (uri[0] == updated_uri):
        print "Setting new values", user_data
        model.set(iter,
                  1, user_data[1],
                  2, user_data[2],
                  3, user_data[3],
                  4, user_data[4])
        return True
    

def notification_update (updated_uris):
    print "Received signal"
    print "%d update: %s" % (len(updated_uris), [str(n) for n in updated_uris])
    for uri in updated_uris:
        details = tracker.get_info_for_entry (uri)
        if (details):
            if (details[2]):
                model_row = (uri, details[0], details[1],
                             details[2], NORMAL)
            else:
                model_row = (uri, details[0], details[1],
                             details[2], BOLD)
            posts_model.foreach (update_uri, model_row)

if __name__ == "__main__":

    dbus_loop = DBusGMainLoop(set_as_default=True)

    bus = dbus.SessionBus (dbus_loop)
    bus.add_signal_receiver (notification_addition,
                             signal_name="SubjectsAdded",
                             dbus_interface="org.freedesktop.Tracker.Resources.Class",
                             path="/org/freedesktop/Tracker/Resources/Classes/nmo/FeedMessage")

    bus.add_signal_receiver (notification_removal,
                             signal_name="SubjectsRemoved",
                             dbus_interface="org.freedesktop.Tracker.Resources.Class",
                             path="/org/freedesktop/Tracker/Resources/Classes/nmo/FeedMessage")

    bus.add_signal_receiver (notification_update,
                             signal_name="SubjectsChanged",
                             dbus_interface="org.freedesktop.Tracker.Resources.Class",
                             path="/org/freedesktop/Tracker/Resources/Classes/nmo/FeedMessage")

    gobject.set_application_name ("Rss/tracker")
    window = gtk.Window ()
    
    vbox = gtk.VBox ()
    posts_treeview = create_posts_tree_view ()
    posts_treeview.set_model (posts_model)
    vbox.add (posts_treeview)

    channels_treeview = create_channels_tree_view ()
    channels_treeview.set_model (channels_model)
    
    dialog = gtk.Dialog ("Sources", window,
                         gtk.DIALOG_MODAL|gtk.DIALOG_DESTROY_WITH_PARENT,
                         (gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT,
                          gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))
    dialog.vbox.add (channels_treeview)


    button_box = gtk.HBox ()
    button_toggle = gtk.Button (label="Read/Unread")
    button_toggle.connect ("clicked", clicked_toggle_cb)
    button_box.add (button_toggle)
    button_sources = gtk.Button (stock=gtk.STOCK_HOME)
    button_sources.connect ("clicked", clicked_sources_cb, dialog)
    button_box.add (button_sources)
    
    vbox.add (button_box)
    window.add (vbox)

    populate_initial_posts ()

    window.show_all ()
    def destroy (window):
        tracker.flush_to_file ()
        gtk.main_quit ()
    
    window.connect ("destroy", destroy)
    gtk.main ()
