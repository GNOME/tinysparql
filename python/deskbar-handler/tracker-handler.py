#!/usr/bin/env python
#    This handler was originaly created by Mikkel Kamstrup and finaly
#    new version it's writed by Eugenio Cutolo(eulin)
#
#    This program can be distributed under the terms of the GNU GPL.
#    See the file COPYING.
#

import gnome
import gobject
from gettext import gettext as _

import re,urllib
import os.path
import dbus

import deskbar
from deskbar.Handler import SignallingHandler
from deskbar.Match import Match

#Edit this var for change the numer of output results
MAX_RESULTS = 2

def _check_requirements ():
	try:
		import dbus
		try :
			if getattr(dbus, 'version', (0,0,0)) >= (0,41,0):
				import dbus.glib
		except:
			return (deskbar.Handler.HANDLER_IS_NOT_APPLICABLE, "Python dbus.glib bindings not found.", None)
		return (deskbar.Handler.HANDLER_IS_HAPPY, None, None)
	except:
		return (deskbar.Handler.HANDLER_IS_NOT_APPLICABLE, "Python dbus bindings not found.", None)

HANDLERS = {
	"TrackerSearchHandler" : {
		"name": "Search for files using Tracker Search Tool",
		"description": _("Search all of your documents (using Tracker), as you type"),
		"requirements" : _check_requirements,
	},
	"TrackerLiveSearchHandler" : {
		"name": "Search for files using Tracker(live result)",
		"description": _("Search all of your documents (using Tracker live), as you type"),
		"requirements" : _check_requirements,
		"categories" : {
			"develop"	: {	
				"name": _("Development Files"),
			},
			"music"	: {	
				"name": _("Music"),
			},
			"images"	: {	
				"name": _("Images"),
			},
			"videos"	: {	
				"name": _("Videos"),
			},
		},
	},
}

#For now description param it's not used
TYPES = {
	"Conversations"	: {
		"description": (_("See  conversations %s") % "<i>%(publisher)s</i>" ) + "\n<b>%(base)s</b>",
		"category": "conversations",
		},
	"Emails"	: {
		"description": (_("See  mail %s") % "<i>%(publisher)s</i>" ) + "\n<b>%(base)s</b>",
		"category": "emails",
		},
	"Music"	: {
		"description": _("Listen music %s\nin %s")	% ("<b>%(base)s</b>", "<i>%(dir)s</i>"),
		"category": "music",
		},
	
	"Documents" 	: {
		"description": _("See document %s\nin %s")	% ("<b>%(base)s</b>", "<i>%(dir)s</i>"),
		"category": "documents",
		},
	"Development Files" 	: {
		"description": _("Open file %s\nin %s")	% ("<b>%(base)s</b>", "<i>%(dir)s</i>"),
		"category": "develop",
		},
	"Images" 		: { 
		"description": _("View image %s\nin %s")	% ("<b>%(base)s</b>", "<i>%(dir)s</i>"),
		"category": "images",
		},
	"Videos"	: {
		"description": _("Watch video  %s\nin %s")	% ("<b>%(base)s</b>", "<i>%(dir)s</i>"),
		"category": "videos",
		},
	"Other Files"	: {
		"description": _("Open file %s\nin %s")	% ("<b>%(base)s</b>", "<i>%(dir)s</i>"),
		"category": "files",
		},
	"Extra"	: {
		"description": _("See more result with t-s-t"),
		},
}


#STATIC HANDLER---------------------------------

class TrackerFileMatch (Match):
	def __init__(self, backend, **args):
		deskbar.Match.Match.__init__(self, backend, **args)
		
	def action(self, text=None):
		gobject.spawn_async(["tracker-search-tool", self.name], flags=gobject.SPAWN_SEARCH_PATH)
			
	def get_verb(self):
		return _("Search <b>"+self.name+"</b> with Tracker Search Tool")
	
	def get_category (self):
		return "actions"

class TrackerSearchHandler(deskbar.Handler.Handler):
	def __init__(self):
		deskbar.Handler.Handler.__init__(self, ("system-search", "tracker"))
				
	def query(self, query):
		return [TrackerFileMatch(self, name=query)]

#LIVE HANDLER---------------------------------

class TrackerMoreMatch (Match):
	def __init__(self, backend, qstring, category="files", **args):
		Match.__init__(self, backend, **args)
		self._icon = deskbar.Utils.load_icon("stock-find")
		self.qstring = qstring
		self.category = category

	def get_verb(self):
		return TYPES["Extra"]["description"]

	def get_category (self):
		try:
			return TYPES[self.category]["category"]
		except:
			pass

	def action(self, text=None):
		gobject.spawn_async(["tracker-search-tool", self.qstring], flags=gobject.SPAWN_SEARCH_PATH)
		
class TrackerLiveFileMatch (Match):
	def __init__(self, handler,result=None, **args):
		Match.__init__ (self, handler,name=result["name"], **args)
		self._icon = deskbar.Utils.load_icon_for_file(result['uri'])
		self.result = result
		self.fullpath = result['uri']
		self.init_names()

	def get_name(self, text=None):
		try:
			return {"base": self.base,"dir": self.dir}
		except:
			pass

	def get_verb(self):
		try:
			return TYPES[self.result["type"]]["description"]
		except:
			return _("Open file %s\nin %s")	% ("<b>%(base)s</b>", "<i>%(dir)s</i>")
	
	def get_hash(self, text=None):
		try:
			return self.result['uri']
		except:
			pass

	def action(self, text=None):
		print "Opening Tracker hit:", self.result['uri']
		gnome.url_show ("file://"+urllib.quote(self.result['uri']))
		
	def get_category (self):
		try:
			return TYPES[self.result["type"]]["category"]
		except:
			return "files"

	def init_names (self):
		#print "Parsing Â«%rÂ»" % self.fullpath
		dirname, filename = os.path.split(self.fullpath)
		if filename == '': #We had a trailing slash
			dirname, filename = os.path.split(dirname)
		
		#Reverse-tilde-expansion
		home = os.path.normpath(os.path.expanduser('~'))
		regexp = re.compile(r'^%s(/|$)' % re.escape(home))
		dirname = re.sub(regexp, r'~\1', dirname)
		
		self.dir = dirname
		self.base = filename


class TrackerLiveSearchHandler(SignallingHandler):
	def __init__(self):
		SignallingHandler.__init__(self, "stock_file")
		bus = dbus.SessionBus()
		self.tracker = bus.get_object('org.freedesktop.Tracker','/org/freedesktop/tracker')
		self.search_iface = dbus.Interface(self.tracker, 'org.freedesktop.Tracker.Search')
		self.keywords_iface = dbus.Interface(self.tracker, 'org.freedesktop.Tracker.Keywords')
		self.files_iface = dbus.Interface(self.tracker, 'org.freedesktop.Tracker.Files')
		self.set_delay (500)
		
	def recieve_hits (self, qstring, hits, max):
		matches = []
		self.results = {}
		print hits
		for info in hits:
			output = {} 
			output['name'] = os.path.basename(info[0])
			output['uri'] = info[0] 
			output['type'] = info[1]
			if TYPES.has_key(output['type']) == 0:
				output['type'] = "Other Files"	
			try:
				self.results[output['type']].append(output)
			except:
				self.results[output['type']] = [output]
		for key in self.results.keys():
				for res in self.results[key][0:MAX_RESULTS]:
					matches.append(TrackerLiveFileMatch(self,res))
				#if len(self.results[key]) > MAX_RESULTS:
				#	matches.append( TrackerMoreMatch(self,qstring,key) )
		self.emit_query_ready(qstring, matches)
		print "Tracker response for %s, - %s hits returned, %s shown" % (qstring, len(hits), len(matches))
		
	def recieve_error (self, error):
		print "*** Tracker dbus error:", error
				
	def query (self, qstring, max):
		if qstring.count("tag:") == 0: 
			self.search_iface.TextDetailed (-1, "Files", qstring, 0,100, reply_handler=lambda hits : self.recieve_hits(qstring, hits, max), error_handler=self.recieve_error)
			print "Tracker query:", qstring
		else:
			if self.tracker.GetVersion() == 502:
				self.search_iface.Query(-1,"Files",["File.Format"],"",qstring.replace("tag:",""),"",False,0,100, reply_handler=lambda hits : self.recieve_hits(qstring, hits, max), error_handler=self.recieve_error)
			elif self.tracker.GetVersion() == 503:
				self.search_iface.Query(-1,"Files",["File.Mime"],"",qstring.replace("tag:",""),"",False,0,100, reply_handler=lambda hits : self.recieve_hits(qstring, hits, max), error_handler=self.recieve_error)
			print "Tracker tag query:", qstring.replace("tag:","")

