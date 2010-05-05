#! /usr/bin/env python
#
# Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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
import sys
import random
import urllib

artist_UID = {}
album_UID = {}
fileid = 0

def printHeader():
	f.write("@prefix nco:   <http://www.semanticdesktop.org/ontologies/2007/03/22/nco#>.\n")
	f.write("@prefix rdfs:  <http://www.w3.org/2000/01/rdf-schema#>.\n")
	f.write("@prefix nrl:   <http://www.semanticdesktop.org/ontologies/2007/08/15/nrl#>.\n")
	f.write("@prefix nid3:   <http://www.semanticdesktop.org/ontologies/2007/05/10/nid3#>.\n")
	f.write("@prefix nmm:   <http://www.tracker-project.org/temp/nmm#>.\n")
	f.write("@prefix nao:   <http://www.semanticdesktop.org/ontologies/2007/08/15/nao#>.\n")
	f.write("@prefix nfo:   <http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#>.\n")
	f.write("@prefix nie:   <http://www.semanticdesktop.org/ontologies/2007/01/19/nie#>.\n");
	f.write("@prefix xsd:   <http://www.w3.org/2001/XMLSchema#>.\n\n")

def generate_name():
        name = os.popen('./generate-name.py').read()

        first_name = ""
        last_name = ""

 	for line in name.splitlines():
                if not first_name:
                        first_name = line
                        continue

                if not last_name:
                        last_name = line
                        continue

        full_name = '%s %s' % (first_name, last_name)

        return full_name

def update_tag(artistid, artistname, albumid, trackid, genreid):
	global fileid

	length = random.randint(5000,5000000)
	song = 'SongTitle%03d' % fileid
	album = 'Album%03d' % albumid
	genre = 'Genre%03d' % genreid
	trackstr = str(artistid) + '/' + str(trackid)
	fullpath = '/home/foo/music/%s/%s/%03d.mp3' % (artistname, album, trackid)
	fileid += 1
	year = '%04d' % random.randint(1950, 2010)
	size = '%03d' % random.randint(3 * 1024, 10 * 1024)
	modified = "%04u-%02u-%02uT15:18:16" % (random.randint(1950, 2010),
                                                random.randint(1, 12),
                                                random.randint(1, 28))
	created = modified

	if not artist_UID.has_key(artistname):
                #print " The new  artist is "+artist
                UID = str(random.randint(0, sys.maxint))
                artist_UID[artistname] = UID
                f.write('<urn:uuid:' + UID + '> a nco:Contact; \n')
		f.write('\tnco:fullname "' + artistname + '".\n\n')
	else:
                UID = artist_UID[artistname]

	if not album_UID.has_key(album):
                album_UID[album] = album
                f.write('<urn:album:' + album + '> a nmm:MusicAlbum; \n')

                if len(UID)>0:
                        f.write('\tnmm:albumArtist <urn:uuid:' + UID + '>;\n')

                f.write('\tnie:title "' + album + '".\n\n')
	else:
                UID = artist_UID[artistname]

        f.write('<file://' + urllib.pathname2url(fullpath) + '> a nmm:MusicPiece,nfo:FileDataObject;\n')
        if len(song) > 0:
                f.write('\tnie:title "' + song + '";\n')

        f.write('\tnfo:fileName \"' + artistname + '.mp3\";\n')
        f.write('\tnfo:fileLastModified "' + modified + '" ;\n')
        f.write('\tnfo:fileCreated "' + created + '";\n')
        f.write('\tnfo:fileSize ' + str(size) + ';\n')
        f.write('\tnmm:musicAlbum <urn:album:' + album + '>;\n')
        f.write('\tnmm:genre "' + genre + '";\n')

        if len(trackstr) > 0:
        	trackArray = trackstr.split("/")
                if len(trackArray) > 0:
                        f.write('\tnmm:trackNumber ' + trackArray[0] + ';\n')

	f.write('\tnfo:duration ' + str(length) + ';\n')
        f.write('\tnmm:performer <urn:uuid:' + UID + '>.\n\n')

def create_track(artistid, albumid, genreid, settings):
        artistname = generate_name()

        for trackid in range(1, settings['TitlesPerAlbum'] + 1):
                update_tag(artistid, artistname, albumid, trackid, genreid)

        genreid += 1
        if genreid > settings['GenreCount']:
                genreid = 1

        return genreid

def generate(settings):
        ''' A total of TotalTracks files will be generated.
	These contain the specified number of albums.'''
        '''
        filepath = settings['OutputDir']
        try:
    	os.makedirs(filepath)
        except:
        print 'Directory exists'
        '''

        global album_UID
        genreid = 1
        artistid = 1
        albumid = 0

        for artistid in range(1, settings['ArtistCount'] + 1):
                album_UID = {}

    	for albums in range(1, settings['AlbumCount'] + 1):
		albumid += 1
        	genreid = create_track(artistid, albumid, genreid, settings)

if __name__ == '__main__':
	settings = {}

	from optparse import OptionParser

	parser = OptionParser()

        parser.add_option("-T", "--TotalTracks",
                          dest='TotalTracks',
                          help="Specify (mandatory) the total number of files to be generated",
                          metavar="TotalTracks")
        parser.add_option("-r", "--ArtistCount",
                          dest='ArtistCount',
                          default=2,
                          help="Specify (mandatory) the total number of Artists." ,
                          metavar="ArtistCount")
        parser.add_option("-a", "--album-count",
                          dest='AlbumCount',
                          default=5,
                          help="Specify (mandatory) the number of albums per artist.",
                          metavar="AlbumCount")
        parser.add_option("-g", "--genre-count",
                          dest='GenreCount',
                          default=10,
                          help="Specify the genre count" ,
                          metavar="GenreCount")
        parser.add_option("-o", "--output",
                          dest='OutputFileName',
                          default='generate-data-for-music.ttl',
                          help="Specify the output ttl filename. e.g. -T 2000 -r 25 -a 20 -g 10 -o generated_songs.ttl",
                          metavar="OutputFileName")

	(options, args) = parser.parse_args()

	mandatories = ['TotalTracks', 'ArtistCount', 'AlbumCount']
	for m in mandatories:
                if not options.__dict__[m]:
                        # Set defaults
                        if m == "TotalTracks":
                                options.TotalTracks = 100
                        elif m == "ArtistCount":
                                options.ArtistCount = 2
                        elif m == "AlbumCount":
                                options.AlbumCount = 10

	settings['TotalTracks'] = options.TotalTracks
	if settings['TotalTracks'] < (options.ArtistCount * options.AlbumCount):
		sys.exit('InputError: TotalTracks should be greater than or equal to  ArtistCount * AlbumCount')

	settings['TitlesPerAlbum'] = int(settings['TotalTracks']) / (int(options.ArtistCount) * int(options.AlbumCount))
	#print 'settings[\'TitlesPerAlbum\'] %d' %settings['TitlesPerAlbum']
	settings['ArtistCount'] = int(options.ArtistCount)
	settings['AlbumCount'] = int(options.AlbumCount)
	settings['GenreCount'] = int(options.GenreCount)
	settings['OutputFileName'] = options.OutputFileName

	f = open(settings['OutputFileName'], 'w' )
	printHeader()
	generate(settings)
