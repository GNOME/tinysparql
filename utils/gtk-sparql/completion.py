# -*- coding: utf-8 -*-
#
# Code taken from http://code.google.com/p/emesene-plugins/
# Specifically http://emesene-plugins.googlecode.com/svn/trunk/Completion.py
#
# The original file doesn't contain License header, but
# the project is under license GPL v2
#
# GtkSparql - Gtk UI to try SparQL queries against tracker.
# Copyright (C) 2009, emesene-plugins project
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

VERSION = '0.1'
import os
import re

import gobject
import gtk

class CompletionEngine:
    def __init__(self):
        self.complete_functions = []

    def add_complete_function(self, function):
        self.complete_functions.append(function)

    def remove_complete_function(self, function):
        self.complete_functions.remove(function)

    def complete_word(self, buffer):
        complete = []
        for func in self.complete_functions:
            new = func(buffer)
            new.reverse()
            complete.extend(new)
        return complete

class CompletionWindow(gtk.Window):
    """Window for displaying a list of completions."""
 
    def __init__(self, parent, callback):
        gtk.Window.__init__(self, gtk.WINDOW_TOPLEVEL)
        self.set_decorated(False)
        self.store = None
        self.view = None
        self.completions = None
        self.complete_callback = callback
        self.set_transient_for(parent)
        self.set_border_width(1)
        self.text = gtk.TextView()
        self.text.set_wrap_mode (gtk.WRAP_WORD)
        self.text_buffer = gtk.TextBuffer()
        self.text.set_buffer(self.text_buffer)
        #self.text.set_size_request(130, 100)
        self.text.set_sensitive(False)
        self.init_tree_view()
        self.init_frame()
        self.cb_ids = {}
        self.cb_ids['focus-out'] = self.connect('focus-out-event', self.focus_out_event)
        self.cb_ids['key-press'] = self.connect('key-press-event', self.key_press_event)
        self.grab_focus()
        self.custom_widget = None

    
    def clear(self):
        self.text_buffer.set_text('')
        if self.custom_widget:
            self.custom_widget.hide()
            self.hbox.remove(self.custom_widget)
            self.custom_widget = None

    def key_press_event(self, widget, event):
        if event.keyval == gtk.keysyms.Escape:
            self.clear()
            self.hide()
            return True
        if event.keyval == gtk.keysyms.BackSpace:
            self.clear()
            self.hide()
            return True
        if event.keyval in (gtk.keysyms.Return, gtk.keysyms.Right):
            self.complete()
            return True
        if event.keyval == gtk.keysyms.Up:
            self.select_previous()
            return True
        if event.keyval == gtk.keysyms.Down:
            self.select_next()
            return True

        char = gtk.gdk.keyval_to_unicode(event.keyval)
        if char:
            self.complete_callback(chr(char))
        return False
 
    def complete(self):
        self.complete_callback(self.completions[self.get_selected()]['completion'])
 
    def focus_out_event(self, *args):
        self.hide()
    
    def get_selected(self):
        """Get the selected row."""
 
        selection = self.view.get_selection()
        return selection.get_selected_rows()[1][0][0]
 
    def init_frame(self):
        """Initialize the frame and scroller around the tree view."""
 
        scroller = gtk.ScrolledWindow()
        scroller.set_policy(gtk.POLICY_NEVER, gtk.POLICY_AUTOMATIC)
        scroller.add(self.view)
        frame = gtk.Frame()
        frame.set_shadow_type(gtk.SHADOW_OUT)
        self.hbox = hbox = gtk.HBox()
        hbox.pack_start(scroller, True, True)
 
        scroller_text = gtk.ScrolledWindow()
        scroller_text.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_NEVER)
        scroller_text.add(self.text)
        hbox.pack_start (scroller_text, True, True)
        frame.add(hbox)
        self.add(frame)

        #self.set_geometry_hints(scroller, min_width=200, min_height=200)
        #self.set_geometry_hints(scroller_text, min_width=200, min_height=200)
        self.set_geometry_hints (scroller_text, min_width=2500)
        self.set_geometry_hints (None, min_width=400, min_height=200)
        
        #self.set_geometry_hints(scroller, max_height=450)
        #self.set_geometry_hints(frame, max_height=2500)
        #self.set_geometry_hints(None, max_height=2500)
 
 
    def init_tree_view(self):
        """Initialize the tree view listing the completions."""
 
        self.store = gtk.ListStore(gobject.TYPE_STRING)
        self.view = gtk.TreeView(self.store)
        renderer = gtk.CellRendererText()
        column = gtk.TreeViewColumn("", renderer, text=0)
        self.view.append_column(column)
        self.view.set_enable_search(False)
        self.view.set_headers_visible(False)
        self.view.set_rules_hint(True)
        selection = self.view.get_selection()
        selection.set_mode(gtk.SELECTION_SINGLE)
        #self.view.set_size_request(100, 60)
        self.view.connect('row-activated', self.row_activated)
 
 
    def row_activated(self, tree, path, view_column, data = None):
        self.complete()
 
 
    def select_next(self):
        """Select the next completion."""
 
        self.clear()
        row = min(self.get_selected() + 1, len(self.store) - 1)
        selection = self.view.get_selection()
        selection.unselect_all()
        selection.select_path(row)
        self.view.scroll_to_cell(row)
        self.show_completions()
 
    def select_previous(self):
        """Select the previous completion."""
 
        self.clear()
        row = max(self.get_selected() - 1, 0)
        selection = self.view.get_selection()
        selection.unselect_all()
        selection.select_path(row)
        self.view.scroll_to_cell(row)
        self.show_completions()
 
    def set_completions(self, completions):
        """Set the completions to display."""
 
        self.completions = completions
        #self.completions.reverse()
        #self.resize(1, 1)
        self.store.clear()
        for completion in completions:
            self.store.append([unicode(completion['abbr'])])
        self.view.columns_autosize()
        self.view.get_selection().select_path(0)
        #self.text_buffer.set_text(self.completions[self.get_selected()]['info'])
        self.show_completions()

    def show_completions(self):
        self.clear()
        completion = self.completions[self.get_selected()]
        info = completion['info']
        if not info:
            self.text.hide()
        else:
            self.text_buffer.set_text(info)
            self.text.show()
 
    def set_font_description(self, font_desc):
        """Set the label's font description."""
 
        self.view.modify_font(font_desc)

class Completer:
    '''This class provides completion feature for ONE conversation'''
    def __init__(self, engine, view, window):
        self.engine = engine
        self.view = view
        self.window = window
        self.popup = CompletionWindow(None, self.complete)

        self.key_press_id = view.connect("key-press-event", self.on_key_pressed)

    def disconnect(self):
        self.view.disconnect(self.key_press_id)

    def complete(self, completion):
        """Complete the current word."""

        doc = self.view.get_buffer()
        doc.insert_at_cursor(completion)
        self.hide_popup()

    def cancel(self):
        self.hide_popup()
        return False

    def hide_popup(self):
        """Hide the completion window."""

        self.popup.hide()
        self.completes = None

    def show_popup(self, completions, x, y):
        """Show the completion window."""
 
        root_x, root_y = self.window.get_position()
        print 'root', root_x, root_y
        self.popup.move(root_x + x + 24, root_y + y + 44)
        self.popup.set_completions(completions)
        self.popup.show_all()

    def display_completions(self, view, event):
        doc = view.get_buffer()
        insert = doc.get_iter_at_mark(doc.get_insert())

        window = gtk.TEXT_WINDOW_TEXT
        rect = view.get_iter_location(insert)
        x, y = view.buffer_to_window_coords(window, rect.x, rect.y)
        x, y = view.translate_coordinates(self.window, x, y)
        completes = self.engine.complete_word(doc)
        if completes:
            self.show_popup(completes, x, y)
            return True
        else:
            return False

    def on_key_pressed(self, view, event):
        if event.keyval == gtk.keysyms.Tab:
        #if event.state & gtk.gdk.CONTROL_MASK and event.state & gtk.gdk.MOD1_MASK and event.keyval == gtk.keysyms.Enter:
            return self.display_completions(view, event)
        if event.state & gtk.gdk.CONTROL_MASK:
            return self.cancel()
        if event.state & gtk.gdk.MOD1_MASK:
            return self.cancel()
        return self.cancel()
