# -*- coding: utf-8 -*-
#    This handler was originaly created by Mikkel Kamstrup (c) 2006 and updated by Eugenio Cutolo (eulin)
#
#    The static search Handler was splitted to a separate file by Marcus Fritzsch
#
#    This program can be distributed under the terms of the GNU GPL version 2 or later.
#    See the file COPYING.
#

import sys
import os.path
import gnome
import gobject

import gettext
gettext.install('tracker')

import deskbar.Handler
import deskbar.Match




class TrackerSearchToolMatch (deskbar.Match.Match):

	def __init__(self, backend, **args):
		deskbar.Match.Match.__init__(self, backend, **args)
		self._icon = deskbar.Utils.load_icon ('tracker')

	def action(self, text=None):
		gobject.spawn_async(['tracker-search-tool', self.name], flags=gobject.SPAWN_SEARCH_PATH)

	def get_verb(self):
		return _('Search for %s with Tracker Search Tool') % ('<b>%(name)s</b>')

	def get_category (self):
		return 'actions'

	def get_hash (self, text=None):
		return 'tst-more-hits-action-'+self.name




class TrackerSearchToolHandler(deskbar.Handler.Handler):

	def __init__(self):
		deskbar.Handler.Handler.__init__(self, 'tracker')

	def query(self, query):
		return [TrackerSearchToolMatch(self, name=query)]

	@staticmethod
	def requirements ():
		if deskbar.Utils.is_program_in_path ('tracker-search-tool'):
			return (deskbar.Handler.HANDLER_IS_HAPPY, None, None)
		return (deskbar.Handler.HANDLER_IS_NOT_APPLICABLE, 'tracker-search-tool seems not to be installed properly.', None)




HANDLERS = {
	'TrackerSearchToolHandler': {
		'name': 'Search for files using Tracker Search Tool',
		'description': _('Search all of your documents with Tracker Search Tool'),
		'requirements': TrackerSearchToolHandler.requirements, # XXX makes deskbar 2.18.1 not load the handler!!
	},
}
