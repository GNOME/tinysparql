import os
import gtk
import dbus

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
			  </menu>
			 </menubar>
			</ui>
		'''
		actions = [
			('Menu',  None, 'Menu'),
			('Search', None, '_Search...', None, 'Search files with MetaTracker', self.on_activate),
			('Preferences', gtk.STOCK_PREFERENCES, '_Preferences...', None, 'Change MetaTracker preferences', self.on_preferences),
			('About', gtk.STOCK_ABOUT, '_About...', None, 'About MetaTracker', self.on_about)]
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

	def on_activate(self, data):
		os.spawnlpe(os.P_NOWAIT, 'tracker-search-tool', os.environ)

	def on_popup_menu(self, status, button, time):
		self.menu.popup(None, None, None, button, time)

	def on_preferences(self, data):
		os.spawnlpe(os.P_NOWAIT, 'tracker-preferences',os.environ)

	def on_about(self, data):
		dialog = gtk.AboutDialog()
		dialog.set_name('MetaTracker')
		dialog.set_version('0.6.0')
		dialog.set_comments('A desktop indexing and search tool')
		dialog.set_website('www.freedesktop.org/Tracker')
		dialog.set_logo(gtk.gdk.pixbuf_new_from_file_at_size('applet.svg', 64, 64))
		dialog.run()
		dialog.destroy()

if __name__ == '__main__':
	TrackerStatusIcon()
	gtk.main()
