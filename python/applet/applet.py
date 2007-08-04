#!/usr/bin/env python

import os
import gtk
import dbus
import gobject

POPUP_TIMEOUT_MILLIS = 3000
POLL_MILLIS = 5000

class Popup (gtk.Window):
	def __init__ (self, status, widget):
		gtk.Window.__init__ (self)
		self.set_decorated (False)
		self.set_skip_taskbar_hint (True)
		self.set_skip_pager_hint (True)
		self.set_keep_above (True)
		self.set_resizable (False)
		label = gtk.Label ("MetaTracker is " + status)
		label.show ()
		ebox = gtk.EventBox ()
		ebox.set_visible_window (True)
		ebox.set_above_child (True)
		ebox.add (label)
		ebox.modify_bg (gtk.STATE_NORMAL, gtk.gdk.Color (65535, 65535, 56576))
		ebox.set_border_width (1)
		ebox.show ()
		self.add (ebox)
		self.show ()
		scr, rect, orient = widget.get_geometry ()
		wdir = (rect.x > scr.get_width () / 2) and -1 or 1
		hdir = (rect.y > scr.get_height () / 2) and -1 or 1
		width, height = self.get_size ()
		self.modify_bg (gtk.STATE_NORMAL, gtk.gdk.Color (0,0,0))
		self.move (rect.x + (width / 2) * wdir, rect.y + (height / 2) * hdir)
		#self.style.paint_flat_box(self.window, gtk.STATE_NORMAL, gtk.SHADOW_OUT, None, self, 'tooltip', 0, 0, width, height)

class TrackerStatusIcon(gtk.StatusIcon):
	def __init__(self):
		gtk.StatusIcon.__init__(self)
		menu = '''
		<ui>
			<menubar name="Menubar">
				<menu action="Menu">
					<menuitem action="Search"/>
					<menuitem action="Preferences"/>
					<separator/>
					<menuitem action="About"/>
					<separator/>
					<menuitem action="Quit"/>
				</menu>
			</menubar>
		</ui>
		'''
		actions = [
			('Menu',	None, 'Menu'),
			('Search', None, '_Search...', None, 'Search files with MetaTracker', self.on_activate),
			('Preferences', gtk.STOCK_PREFERENCES, '_Preferences...', None, 'Change MetaTracker preferences', self.on_preferences),
			('About', gtk.STOCK_ABOUT, '_About...', None, 'About MetaTracker', self.on_about),
			('Quit', gtk.STOCK_QUIT, '_Quit...', None, 'Quit Status Applet', gtk.main_quit)]
		ag = gtk.ActionGroup('Actions')
		ag.add_actions(actions)
		self.manager = gtk.UIManager()
		self.manager.insert_action_group(ag, 0)
		self.manager.add_ui_from_string(menu)
		self.menu = self.manager.get_widget('/Menubar/Menu/About').props.parent
		search = self.manager.get_widget('/Menubar/Menu/Search')
		search.get_children()[0].set_markup('<b>_Search...</b>')
		search.get_children()[0].set_use_underline(True)
		search.get_children()[0].set_use_markup(True)
		pixbuf = gtk.gdk.pixbuf_new_from_file_at_size('applet.svg', 16, 16)
		search.get_children()[1].set_from_pixbuf(pixbuf)
		self.set_from_file('applet.svg')
		self.set_tooltip('MetaTracker Desktop Search')
		self.set_visible(True)
		self.connect('activate', self.on_activate)
		self.connect('popup-menu', self.on_popup_menu)
		self.old_status = ""
		self.connectToDBus ()
		gobject.timeout_add (0, self.check_tracker_state)

	def connectToDBus (self):
		self.bus = dbus.SessionBus ()
		self.connectToTracker ()

	def connectToTracker (self):
		self.obj = self.bus.get_object('org.freedesktop.Tracker','/org/freedesktop/tracker')
		self.tracker = dbus.Interface(self.obj, 'org.freedesktop.Tracker')

	def getTrackerStatus (self):
		st = ""
		try: st = str (self.tracker.GetStatus ())
		except:
			st = "Unreachable!"
			try: self.connectToTracker ()
			except: pass
		return st

	def check_tracker_state (self):
		stat = self.getTrackerStatus ()
		if stat != self.old_status:
			p = Popup (stat, self)
			gobject.timeout_add (POPUP_TIMEOUT_MILLIS, p.destroy)
			self.old_status = stat
		self.set_tooltip ("MetaTracker is " + stat)
		gobject.timeout_add (POLL_MILLIS, self.check_tracker_state)

	def on_activate(self, data):
		tst="tracker-search-tool"
		os.spawnlp (os.P_NOWAIT, tst, tst)

	def on_popup_menu(self, status, button, time):
		self.menu.popup(None, None, None, button, time)

	def on_preferences(self, data):
		tp="tracker-preferences"
		os.spawnlp (os.P_NOWAIT, tp, tp)

	def on_about(self, data):
		dialog = gtk.AboutDialog()
		dialog.set_name('MetaTracker')
		dialog.set_version('0.6.0')
		dialog.set_comments('A desktop indexing and search tool')
		dialog.set_website('http://www.tracker-project.org/')
		dialog.set_logo(gtk.gdk.pixbuf_new_from_file_at_size('applet.svg', 64, 64))
		dialog.run()
		dialog.destroy()

if __name__ == '__main__':
	TrackerStatusIcon()
	gtk.main()
