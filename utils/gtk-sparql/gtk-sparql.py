#!/usr/bin/env python

# GtkSparql - Gtk UI to try SparQL queries against tracker.
# Copyright (C) 2009, Ivan Frade <ivan.frade@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

import gtk
import dbus
import store
import time
import completion
import completion_engine

TRACKER = 'org.freedesktop.Tracker1'
TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources'
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources"

class GtkSparql:

    def __init__ (self, ui):
        self.ui = ui
        self.store = store.QueriesStore ()
        self.saved_queries_model = self.load_saved_queries ()

        combo = ui.get_object ("queries_combo")
        combo.set_model (self.saved_queries_model)
        render = gtk.CellRendererText ()
        combo.pack_start (render, True)
        combo.add_attribute (render, "text", 0)
        combo.connect ("changed", self.select_store_query_cb)

        self.textbuffer = gtk.TextBuffer ()
        self.textbuffer.set_text ("SELECT \nWHERE { \n\n}")
        ui.get_object ("query_text").set_buffer (self.textbuffer)

        self.completer = completion.Completer (completion_engine.TrackerCompletionEngine (),
                                               ui.get_object ("query_text"),
                                               ui.get_object ("main_window"))


        ui.get_object ("save_button").connect ("clicked", self.save_query_cb)
        ui.get_object ("run_button").connect ("clicked", self.execute_query_cb)
        ui.get_object ("delete_button").connect ("clicked", self.delete_store_query_cb)

        # Clean treeview columns
        treeview = ui.get_object ("results_tv")
        columns = treeview.get_columns ()
        for c in columns:
            treeview.remove_column (c)

        self.connect_tracker ()

    def run (self):
        w = builder.get_object ("main_window")
        w.connect ("destroy", gtk.main_quit)
        w.show_all ()
        gtk.main ()

    def connect_tracker (self):
        # TODO Check if the connection is valid
        bus = dbus.SessionBus ()
        tracker = bus.get_object (TRACKER, TRACKER_OBJ)
        self.resources = dbus.Interface (tracker,
                                         dbus_interface=RESOURCES_IFACE);


    def execute_query_cb (self, widget, call=0):
        label = self.ui.get_object ("info_label")
        
        query = self.textbuffer.get_text (self.textbuffer.get_start_iter (),
                                          self.textbuffer.get_end_iter ())
        try:
            start = time.time ()
            result = self.resources.SparqlQuery (query)
            end = time.time ()
            
            label.set_text ("Query took %f sec." % (end-start))
            
            self.set_results_in_treeview (result)
            
        except dbus.exceptions.DBusException, e:
            if ("org.freedesktop.DBus.Error.ServiceUnknown" in str(e) and call == 0):
                self.connect_tracker ()
                self.execute_query_cb (widget, 1)
            else:
                label.set_text (str(e).partition(':')[2])

        
    def set_results_in_treeview (self, result_set):
        if (len (result_set) < 1):
            return None

        columns = len (result_set[0])
        params = tuple (columns * [str])
        store = gtk.ListStore (*params)
        self.configure_tree_view (columns)
        
        counter = 0
        for r in result_set:
            if (counter < 500):
                counter += 1
                store.append (r)
            else:
                label = self.ui.get_object ("info_label")
                current_text = label.get_text ()
                label.set_text (current_text +
                                           "(More than 500 results. Showing only first 500)")
                break

        self.ui.get_object ("results_tv").set_model (store)

    def configure_tree_view (self, columns):
        """
        Add of remove columns in the current treeview to have exactly 'columns'
        """
        treeview = self.ui.get_object ("results_tv")
        current_columns = treeview.get_columns ()
        if (len (current_columns) == columns):
            return

        if (len (current_columns) > columns):
            for i in range (columns, len (current_columns)):
                treeview.remove_column (current_columns [i])

        
        # Add columns. Is the only chance at this moment
        for i in range (len (current_columns), columns):
            render = gtk.CellRendererText() 
            column = gtk.TreeViewColumn ("%d" % (i),
                                         render,
                                         text=i) # primera columna de datos
            treeview.append_column (column)


    def save_query_cb (self, widget):
        dialog = gtk.Dialog ("Save query...",
                             None,
                             gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                             (gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT,
                              gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))
        vbox = dialog.get_content_area ()

        query = self.textbuffer.get_text (self.textbuffer.get_start_iter (),
                                          self.textbuffer.get_end_iter ())
        dialog_query_label = gtk.Label (query)
        vbox.pack_start (dialog_query_label)
        dialog_name_entry = gtk.Entry ()
        vbox.pack_start (dialog_name_entry)
        vbox.show_all ()
        
        # Disable 'ok' button unless there is a name
        dialog.set_response_sensitive (gtk.RESPONSE_ACCEPT, False)
        def validate_entry (entry, dialog):
            dialog.set_response_sensitive (gtk.RESPONSE_ACCEPT, len (entry.get_text ()) > 0)
        dialog_name_entry.connect ("changed", validate_entry, dialog)
        
        response = dialog.run ()
        if (response == gtk.RESPONSE_ACCEPT):
            name = dialog_name_entry.get_text ()
            self.save_query (name, query)
        dialog.destroy ()


    def select_store_query_cb (self, combobox):
        it = combobox.get_active_iter ()
        selected = combobox.get_model ().get (it, 1)[0]
        self.textbuffer.set_text (selected)


    def delete_store_query_cb (self, button):
        combo = self.ui.get_object ("queries_combo")
        it = combo.get_active_iter ()
        name = self.saved_queries_model.get (it, 0)[0]
        self.store.delete_query (name)
        self.saved_queries_model.remove (it)
        combo.set_active (0)


    def load_saved_queries (self):
        queries_model = gtk.ListStore (str, str)
        queries = self.store.get_all_queries ()
        for row in queries:
            queries_model.append (row)
        return queries_model


    def save_query (self, name, value):
        self.store.save_query (name, value)
        it = self.saved_queries_model.append ((name, value))
        self.ui.get_object ("queries_combo").set_active_iter (it)

        
if __name__ == "__main__":

    builder = gtk.Builder ()
    builder.add_from_file ("gtk-sparql.ui")

    app = GtkSparql (builder)
    app.run ()
