#! /usr/bin/env python

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

def updatetag(artistid, albumid, trackid, genreid):

	global fileid
	
	length = 0
	length=random.randint(5000,5000000 )
	song = 'SongTitle [%03u]' % fileid 
	artist = 'TrkArtist [%03u]' % artistid
	album = 'TrkAlbum [%03u]' % albumid
	genre = 'Genre-[%03u]' %genreid
	trackstr = str(artistid) + '/' + str(trackid)
	fullpath = '/home/abc/d/e/%03u.mp3' %fileid
	fileid +=1
	year = '2009'

	size ='%03u' %fileid
	modified = "2009-07-17T15:18:16"
	created = "2009-07-17T15:18:16"
	
	if not artist_UID.has_key(artist):
                #print " The new  artist is "+artist
                UID = str(random.randint(0, sys.maxint))
                artist_UID[artist] = UID
                f.write('<urn:uuid:'+UID+'> a nco:Contact; \n')
		f.write('\tnco:fullname "'+artist+'".\n\n')


	else :
                UID = artist_UID[artist]

	if not album_UID.has_key(album):
                album_UID[album] = album
                f.write('<urn:album:'+album+'> a nmm:MusicAlbum; \n')
		
                if len(UID)>0: f.write('\tnmm:albumArtist <urn:uuid:'+UID+'>;\n')
                f.write('\tnie:title "'+album+'".\n\n')

	else :
                UID = artist_UID[artist]


        f.write('<file://'+urllib.pathname2url(fullpath)+'> a nmm:MusicPiece,nfo:FileDataObject;\n')
        if len(song)>0:f.write('\tnie:title "'+song+'";\n')
        f.write('\tnfo:fileName \"%03u.mp3\";\n' %fileid)
        f.write('\tnfo:fileLastModified "'+modified+'" ;\n')
        f.write('\tnfo:fileCreated "'+created+'";\n')
        f.write('\tnfo:fileSize '+str(size)+';\n')
        f.write('\tnmm:musicAlbum <urn:album:'+album+'>;\n')
        f.write('\tnmm:genre "'+genre+'";\n')
        if len(trackstr)>0:
        	trackArray=trackstr.split("/")
                if len(trackArray)>0: f.write('\tnmm:trackNumber '+trackArray[0]+';\n')


	f.write('\tnmm:length '+str(length)+';\n')
        f.write('\tnmm:performer <urn:uuid:'+UID+'>.\n\n')



def create_track( artistid, albumid, genreid, settings):
    for trackid in range(1, settings['TitlesPerAlbum'] + 1):
        updatetag(artistid, albumid, trackid, genreid)
        genreid += 1
        if genreid > settings['GenreCount']:
            genreid = 1
    return   genreid


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
		albumid+=1
        	genreid = create_track(artistid, albumid, genreid, settings)


if __name__ == '__main__':
	settings = {}

	from optparse import OptionParser

	parser = OptionParser()

        parser.add_option("-T", "--TotalTracks", dest='TotalTracks',
                        help="Specify (mandatory) the total number of files to be generated" , metavar="TotalTracks")
        parser.add_option("-r", "--ArtistCount", dest='ArtistCount', default=2,
                        help="Specify (mandatory) the total number of Artists." , metavar="ArtistCount")
        parser.add_option("-a", "--album-count", dest='AlbumCount', default=5,
                        help="Specify (mandatory) the number of albums per artist." , metavar="AlbumCount")
        parser.add_option("-g", "--genre-count", dest='GenreCount', default=10,
                        help="Specify the genre count" , metavar="GenreCount")
        parser.add_option("-o", "--output", dest='OutputFileName', default='songlistDirect.ttl',
                        help="Specify the output ttl filename. \
			      E.g., -T 2000 -r 25 -a 20 -g 10 -o generated_songs.ttl" , metavar="OutputFileName")

	(options, args) = parser.parse_args()
	
	mandatories = ['TotalTracks', 'ArtistCount', 'AlbumCount']  
	for m in mandatories:  
	    if not options.__dict__[m]:  
	         print "\nMandatory options  missing\n"  
	         parser.print_help()  
	         sys.exit(-1)  

	settings['TotalTracks'] = int(options.TotalTracks)
	if settings['TotalTracks'] < (int(options.ArtistCount) * int(options.AlbumCount) ):
		sys.exit('InputError: TotalTracks should be greater than or equal to  ArtistCount * AlbumCount')


	settings['TitlesPerAlbum'] = settings['TotalTracks'] / (int(options.ArtistCount) * int(options.AlbumCount))
	#print 'settings[\'TitlesPerAlbum\'] %d' %settings['TitlesPerAlbum']
	settings['ArtistCount'] = int(options.ArtistCount)
	settings['AlbumCount'] = int(options.AlbumCount)
	settings['GenreCount'] = int(options.GenreCount)
	settings['OutputFileName'] = options.OutputFileName

	print '\n'+str(settings)+'\n'

	f = open(settings['OutputFileName'], 'w' )
	printHeader()
	generate(settings)




