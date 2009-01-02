#  Copyright (C) 2006/7, Edward B. Duffy <eduffy@gmail.com>
#  tracker-tags-tab.py:  Tag your files in your Tracker database
#                        via Nautilus's property dialog.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc.,  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA


if __name__ == '__main__':
   import os
   import sys

   print 'This is nautilus extension, not a standalone application.'
   print 'Please copy this file into your nautulis extensions directory:'
   print
   print '\t# cp %s %s/.nautilus/python-extensions' % \
               (__file__,os.path.expanduser('~'))

   sys.exit(1)


import gtk
import dbus
import urllib
import operator
import nautilus

class TrackerTagsPage(nautilus.PropertyPageProvider):

   def __init__(self):
      bus = dbus.SessionBus()
      obj = bus.get_object('org.freedesktop.Tracker',
                           '/org/freedesktop/Tracker')
      self.tracker = dbus.Interface(obj, 'org.freedesktop.Tracker')

      obj_kw = bus.get_object('org.freedesktop.Tracker',
                              '/org/freedesktop/Tracker/Keywords')
      self.keywords = dbus.Interface(obj_kw, 'org.freedesktop.Tracker.Keywords')

   def _on_toggle(self, cell, path, files):
      on = not self.store.get_value(self.store.get_iter(path), 0)
      self.store.set_value(self.store.get_iter(path), 0, on)
      tag = self.store.get_value(self.store.get_iter(path), 1)
      if on: func = self.keywords.Add
      else:  func = self.keywords.Remove
      for f in files:
         func('Files', f, [tag])

   def _on_add_tag(self, button):
      self.store.append([False, ''])

   def _on_edit_tag(self, cell, path, text, files):
      old_text = self.store.get_value(self.store.get_iter(path), 1)
      on = self.store.get_value(self.store.get_iter(path), 0)
      if on:
         for f in files:
            self.keywords.Remove('Files', f, [old_text])
            self.keywords.Add('Files', f, [text])
      self.store.set_value(self.store.get_iter(path), 1, text)

   def _on_update_tag_summary(self, store, path, iter):
      tags = [ ]
      for row in store:
         if row[0]:
            tags.append(row[1])
      self.entry_tag.handler_block(self.entry_changed_id)
      self.entry_tag.set_text(','.join(tags))
      self.entry_tag.handler_unblock(self.entry_changed_id)

   def _on_tag_summary_changed(self, entry, files):
      new_tags = set(entry.get_text().split(','))
      new_tags.discard('') # remove the empty string
      for f in files:
         old_tags = set(self.keywords.Get('Files', f))
         tbr = list(old_tags.difference(new_tags))
         tba = list(new_tags.difference(old_tags))
         if tbr:
            self.keywords.Remove('Files', f, tbr)
         if tba:
            self.keywords.Add('Files', f, tba)

      # update check-box list (remove outdated tags, add the new ones)
      self.store.handler_block(self.store_changed_id)
      all_tags = [ t for t,c in self.keywords.GetList('Files') ]
      i = 0
      while i < len(self.store):
         if self.store[i][1] in all_tags:
            self.store[i][0] = (self.store[i][1] in new_tags)
            all_tags.remove(self.store[i][1])
            i += 1
         else:
            del self.store[i]
      #  assert len(all_tags) == 1 ???
      for t in all_tags:
         self.store.append([True, t])
      self.store.handler_unblock(self.store_changed_id)

   def get_property_pages(self, files):
      property_label = gtk.Label('Tags')
      property_label.show()

      # get the list of tags
      all_tags = self.keywords.GetList('Files')
      # convert usage count to an integer
      all_tags = [ (t,int(c)) for t,c in all_tags ]
      # sort by usage count
      all_tags = sorted(all_tags, key=operator.itemgetter(1))
      all_tags.reverse()
      # strip away usage count
      all_tags = [ t for t,c in all_tags ]

      files = [ urllib.url2pathname(f.get_uri()[7:]) for f in files ]
      indiv_count = dict([ (t,0) for t in all_tags ])
      tags = { }
      for f in files:
         tags[f] = self.keywords.Get('Files', f)
         for t in tags[f]:
            indiv_count[t] += 1

      main = gtk.VBox()

      hbox = gtk.HBox()
      hbox.set_border_width(6)
      hbox.set_spacing(12)
      label = gtk.Label('Tags: ')
      self.entry_tag = gtk.Entry()
      # self.entry_tag.props.editable = False
      hbox.pack_start(label, False, False)
      hbox.pack_start(self.entry_tag, True, True)
      main.pack_start(hbox, False, False)
      self.entry_changed_id = self.entry_tag.connect(
                  'changed', self._on_tag_summary_changed, files)

      sw = gtk.ScrolledWindow()
      sw.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
      sw.set_shadow_type(gtk.SHADOW_ETCHED_OUT)

      self.store = gtk.ListStore(bool, str)
      self.store_changed_id = self.store.connect(
                  'row-changed', self._on_update_tag_summary)
      for tag in all_tags:
         iter = self.store.append([False, tag])
         if indiv_count[tag] == len(files):
            self.store.set_value(iter, 0, True)
         elif indiv_count[tag] == 0:
            self.store.set_value(iter, 0, False)
         else:
            print 'inconsistant'
      tv = gtk.TreeView(self.store)
      tv.set_headers_visible(False)

      column = gtk.TreeViewColumn()
      tv.append_column(column)
      cell = gtk.CellRendererToggle()
      column.pack_start(cell, True)
      column.add_attribute(cell, 'active', 0)
      # column.add_attribute(cell, 'inconsistent', 0)
      cell.connect('toggled', self._on_toggle, files)
      cell.set_property('activatable', True)
      # cell.set_property('inconsistent', True)

      column = gtk.TreeViewColumn()
      tv.append_column(column)
      cell = gtk.CellRendererText()
      column.pack_start(cell, True)
      column.add_attribute(cell, 'text', 1)
      cell.connect('edited', self._on_edit_tag, files)
      cell.set_property('editable', True)

      sw.add(tv)
      main.pack_start(sw)

      hbox = gtk.HBox()
      hbox.set_border_width(6)
      btn = gtk.Button(stock='gtk-add')
      btn.get_child().get_child().get_children()[1].props.label = '_Add Tag'
      btn.connect('clicked', self._on_add_tag)
      hbox.pack_end(btn, False, False)
      main.pack_start(hbox, False, False)
      
      main.show_all()

      return nautilus.PropertyPage("NautilusPython::tags", property_label, main),

