#
# Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

import os
import urllib
import sys
import re
from xml.dom import minidom

LYRIC_TITLE_STRIP = ["\(live[^\)]*\)", "\(acoustic[^\)]*\)", "\([^\)]*mix\)", "\([^\)]*version\)", "\([^\)]*edit\)", "\(feat[^\)]*\)"]
LYRIC_TITLE_REPLACE = [("/", "-"), (" & ", " and ")]
LYRIC_ARTIST_REPLACE = [("/", "-"), (" & ", " and ")]

def get_lyrics(artist, title):
	# replace ampersands and the like
	for exp in LYRIC_ARTIST_REPLACE:
		p = re.compile (exp[0])
		artist = p.sub(exp[1], artist)
	for exp in LYRIC_TITLE_REPLACE:
		p = re.compile (exp[0])
		title = p.sub(exp[1], title)

	# strip things like "(live at Somewhere)", "(accoustic)", etc
	for exp in LYRIC_TITLE_STRIP:
		p = re.compile (exp)
		title = p.sub ('', title)

	# compress spaces
	title = title.strip()
	artist = artist.strip()

	url = "http://api.leoslyrics.com/api_search.php?auth=Rhythmbox&artist=%s&songtitle=%s" % (
		urllib.quote(artist.encode('utf-8')),
		urllib.quote(title.encode('utf-8')))

	data = urllib.urlopen(url).read()
	xmldoc = minidom.parseString(data).documentElement

	result_code = xmldoc.getElementsByTagName('response')[0].getAttribute('code')
	if result_code != '0':
		xmldoc.unlink()
		return

	matches = xmldoc.getElementsByTagName('result')[:10]
	hids = map(lambda x: x.getAttribute('hid'), matches)
	if len(hids) == 0:
		self.callback("Unable to find lyrics for this track.")
		xmldoc.unlink()
		return

	xmldoc.unlink()

	url = "http://api.leoslyrics.com/api_lyrics.php?auth=Rhythmbox&hid=%s" % (urllib.quote(hids[0].encode('utf-8')))
	data = urllib.urlopen(url).read()
	xmldoc = minidom.parseString(data).documentElement

	text = xmldoc.getElementsByTagName('title')[0].firstChild.nodeValue
	text += ' - ' + xmldoc.getElementsByTagName('artist')[0].getElementsByTagName('name')[0].firstChild.nodeValue + '\n\n'
	text += xmldoc.getElementsByTagName('text')[0].firstChild.nodeValue

	xmldoc.unlink()
	return text


if __name__ == '__main__':
	if len(sys.argv) != 4:
		print 'usage: %s artist title output' % (sys.argv[0])
		sys.exit(1)

	f = open(sys.argv[3], 'w')
	lyrics = get_lyrics(sys.argv[1], sys.argv[2])
	f.write(lyrics.encode('utf-8'))
	f.close()
