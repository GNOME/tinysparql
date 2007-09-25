#!/usr/bin/env python
#    Created by Eugenio Cutolo  <me@eugesoftware.com>
#
#    This program can be distributed under the terms of the GNU GPL.
#    See the file COPYING.
#

#QtTracker Relase 0.1

import sys,os,dbus,re
from kdecore import KApplication, KCmdLineArgs, KURL,KIconTheme,KIcon
from kio import KMimeType, KServiceTypeProfile
from mainform import *

class TrackerClient:

	def __init__(self):
		bus = dbus.SessionBus()
		obj = bus.get_object('org.freedesktop.Tracker','/org/freedesktop/tracker')
		#self.tracker = dbus.Interface(obj, 'org.freedesktop.Tracker')
		self.keywords_iface = dbus.Interface(obj, 'org.freedesktop.Tracker.Keywords')
		self.search_iface = dbus.Interface(obj, 'org.freedesktop.Tracker.Search')
		self.files_iface = dbus.Interface(obj, 'org.freedesktop.Tracker.Files')
		
		#self.version = self.tracker.GetVersion()

		#Self service eheheh
		#self.services = self.tracker.GetServices(True)
		self.services = ['Files','Development Files','Documents','Images','Music','Text Files','Videos']
		self.query_id = 0

	def search(self,text,service='Files',offset=0,max_hits=-1):
		self.returnedfiles = self.search_iface.TextDetailed(1,service,text,offset,max_hits)
		if len(self.returnedfiles) > 0:
			return self.returnedfiles
		else:
			self.on_tracker_error("Nothing files found")
			return 0
	
	def search_by_tag(self,tag,service='Files',offset=0,max_hits=-1):
		self.returnedfiles = self.keywords_iface.Search(-1,service,[tag,] ,offset,max_hits)
		output = []
		for path in self.returnedfiles:
			output.append([path,self.files_iface.GetServiceType(path),'mime'])
		self.returnedfiles = output
		return output

#Thank you Mikkel
	def text_snippet(self, text, result, service='Files'):
		snippet = self.search_iface.GetSnippet(service, result, text)
		snippet = snippet.replace('<!--', '&lt;!--').strip()
		return snippet

	def on_tracker_error(self, e):
		print "Error:",e

class TrackerGui(MainForm,TrackerClient):
	
	def __init__(self):
		MainForm.__init__(self)
		TrackerClient.__init__(self)
		self.searchinp.setFocus();
		self.fatalerr = 0
		self.mip = 6#Max item for page

	def refresh_page(self):
		self.result_list.clear()
		
		start = (self.mip*self.pagen)
		end = (self.mip*self.pagen)+self.mip + 1

		self.setCursor(QCursor(3))
		if self.mode_combo.currentItem() == 0:
			self.search(self.input,self.service,start,self.mip+1)
		elif self.mode_combo.currentItem() == 1:
			self.search_by_tag(self.input,self.service,start,self.mip+1)
		
		if self.fatalerr:
			return
		
		if self.pagen > 0:
			self.prevbtn.setEnabled(1)
		elif self.pagen == 0:
			self.prevbtn.setEnabled(0)
		if len(self.returnedfiles) > self.mip:
			self.nextbtn.setEnabled(1)
			self.returnedfiles.pop()
		else:
			self.nextbtn.setEnabled(0)

		self.pagen_display.setText("Results "+str(start+1)+" - "+str(end-1))
		self.show_result(self.returnedfiles)

	def show_result(self,result):
		self.result_list.setSorting(-1)
		self.returnedfiles.reverse()
		for (path,service,mime) in result:
			item = QRichListViewItem(self.result_list,None)
			item.setMultiLinesEnabled(1)
			if len(os.path.dirname(path)) > 40:
				dirname = os.path.dirname(path)[0:40]+"..."
			else:
				dirname = os.path.dirname(path)
			item.setText(0,os.path.basename(path)+"\n"+dirname+"\n"+service)
			item.setPixmap(0,self._get_iconc(path))
			item.setText(1,self.text_snippet(self.input,path))
		self.setCursor(QCursor(0))
		self.returnedfiles.reverse()

	def _get_iconc(self,path):
		mobj = KMimeType.findByPath(path)
		return mobj.pixmap(KURL(""),KIcon .Desktop,48)
	
	def on_tracker_error(self, e):
		print "Error:",e
		self.result_list.clear()
		self.pagen_display.setText(e)
		self.fatalerr = 1

	def exec_file(self):
		item = self.result_list.currentItem()
		npos = (item.itemPos() / item.totalHeight())
		mime = KMimeType.findByPath(self.returnedfiles[npos][0]).name()
		offer = KServiceTypeProfile.preferredService(mime, "Application")
		offer = re.sub('%.','',str(offer.exec_ ()))
		print offer+" "+self.returnedfiles[npos][0]
		os.system(offer+" '"+self.returnedfiles[npos][0]+"' &")
		
#----------------Qt Events----------------------------
	
	def findbtn_clicked(self):
		self.pagen = 0
		self.input = str(self.searchinp.text())
		self.service = self.services[self.services_combo.currentItem()]
		self.prevbtn.setEnabled(0)
		self.nextbtn.setEnabled(0)
		self.refresh_page()

	def nextbtn_clicked(self):
		self.pagen = self.pagen + 1
		self.refresh_page()

	def prevbtn_clicked(self):
		self.pagen = self.pagen - 1
		self.refresh_page()

	def result_list_doubleClicked(self,item):
		self.exec_file()

	def result_list_contextMenuRequested(self,item):
		if item != self.result_list:
			contextMenu = QPopupMenu(self)
			contextMenu.insertItem( "&Open", self.exec_file, Qt.CTRL+Qt.Key_O )
			contextMenu.insertItem( "&Open with...", self.exec_file, Qt.CTRL+Qt.Key_S )
			contextMenu.exec_loop( QCursor.pos() )

if __name__ == "__main__":
	KCmdLineArgs.init(sys.argv, "qttrackergui", "qtgui", "0.1")
	app = KApplication()
	gui = TrackerGui()
	gui.show()
	app.setMainWidget(gui)
	app.exec_loop()