#!/usr/bin/python
#
# Copyright (C) 2007, Urho Konttori <urho.konttori@gmail.com>
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

import  os, datetime, time, internals.id3reader as id3reader
import sys, urllib, random
indexPath="/Volumes/OSX"

if len(sys.argv)>1:
     indexPath=str(sys.argv[1])
else:
    print "Usage: python id32nmmTurtle.py <path-to-index>."
    sys.exit (1)


songcounter=0

filelist=[]
folderlist=[]
foldermap=[]
depth=0
artist_UID = {}
album_UID = {}

class FileProcessor:

    def __init__(self):
        self.f=open("./songlist.ttl", 'w' )

        self.f.write("@prefix nco:   <http://www.semanticdesktop.org/ontologies/2007/03/22/nco#>.\n")
        self.f.write("@prefix rdfs:  <http://www.w3.org/2000/01/rdf-schema#>.\n")
        self.f.write("@prefix nrl:   <http://www.semanticdesktop.org/ontologies/2007/08/15/nrl#>.\n")
        self.f.write("@prefix nid3:   <http://www.semanticdesktop.org/ontologies/2007/05/10/nid3#>.\n")
        self.f.write("@prefix nmm:   <http://www.tracker-project.org/temp/nmm#>.\n")
        self.f.write("@prefix nao:   <http://www.semanticdesktop.org/ontologies/2007/08/15/nao#>.\n")
        self.f.write("@prefix nfo:   <http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#>.\n")
        self.f.write("@prefix nie:   <http://www.semanticdesktop.org/ontologies/2007/01/19/nie#>.\n");
        self.f.write("@prefix xsd:   <http://www.w3.org/2001/XMLSchema#>.\n")

    def addMp3(self, fullpath, fileName):
        global songcounter
	global g_UID
        try:
            year=""
            album=""
            song=""
            artist=""
            trackstr=""
            genre=""
            comment=""
            year=""
            length=0
            id3r = id3reader.Reader(fullpath)
            if id3r.getValue('album'):      album  = id3r.getValue('album')
            if id3r.getValue('title'):      song   = id3r.getValue('title')
            if id3r.getValue('performer'):  artist = id3r.getValue('performer')
            if id3r.getValue('year'):       year = id3r.getValue('year')
            if id3r.getValue('genre'):       genre = id3r.getValue('genre')
            if id3r.getValue('comment'):       comment = id3r.getValue('comment')
            length=random.randint(5000,5000000 )
            if id3r.getValue('track'):
                trackstr=str(id3r.getValue('track'))
            if id3r.getValue('TPA'):
                partOfSet=id3r.getValue('TPA')
                if partOfSet=="None": partOfSet=""
            modified=time.strftime("%Y-%m-%dT%H:%M:%S",time.localtime(os.path.getmtime(fullpath)))
            created=time.strftime("%Y-%m-%dT%H:%M:%S",time.localtime(os.path.getctime(fullpath)))
            size = os.path.getsize(fullpath)


            artistUID = ""
            albumUID = ""
            UID=""
            if not artist_UID.has_key(artist):
                 #print " The new  artist is "+artist
                 UID = str(random.randint(0, sys.maxint))
                 artist_UID[artist] = UID
                 self.f.write('<urn:uuid:'+UID+'> a nco:Contact; \n')
                 #self.f.write('<urn:artist:'+artist+'> a nco:Contact; \n')
                 self.f.write('\tnco:fullname "'+artist+'".\n\n')
            else:
                #print 'Artist exists ' + artist
                 UID = artist_UID[artist]

            if not album_UID.has_key(album):
                 #print " The new  album is "+artist

                 album_UID[artist] = album
                 self.f.write('<urn:album:'+album+'> a nmm:MusicAlbum; \n')

                 if len(partOfSet)>0:
                      setArray=partOfSet.split("/")
                      if len(setArray)>0: self.f.write('\tnmm:setNumber '+setArray[0]+';\n')
                      if len(setArray)>1: self.f.write('\tnmm:setCount '+setArray[1]+';\n')
                 if len(UID)>0: self.f.write('\tnmm:albumArtist <urn:uuid:'+UID+'>;\n')
                 self.f.write('\tnie:title "'+album+'".\n\n')
            else:
                 #print 'Artist exists ' + artist
                 UID = artist_UID[artist]

            self.f.write('<file://'+urllib.pathname2url(fullpath)+'> a nmm:MusicPiece,nfo:FileDataObject;\n')
            if len(song)>0: self.f.write('\tnie:title "'+song+'";\n')
            if len(fileName)>0: self.f.write('\tnfo:fileName "'+fileName+'";\n')
            if len(modified)>0: self.f.write('\tnfo:fileLastModified "'+modified+'" ;\n')
            if len(created)>0: self.f.write('\tnfo:fileCreated "'+created+'";\n')
            self.f.write('\tnfo:fileSize '+str(size)+';\n')
            if len(album)>0: self.f.write('\tnmm:musicAlbum <urn:album:'+album+'>;\n')
#            if len(year)>0: self.f.write('\tnid3:recordingYear '+str(year)+';\n')
            if len(genre)>0: self.f.write('\tnmm:genre "'+genre+'";\n')
            if len(trackstr)>0:
                 trackArray=trackstr.split("/")
                 if len(trackArray)>0: self.f.write('\tnmm:trackNumber '+trackArray[0]+';\n')


            if length>0: self.f.write('\tnfo:duration '+str(length)+';\n')
            if len(UID)>0: self.f.write('\tnmm:performer <urn:uuid:'+UID+'>.\n\n')


            songcounter+=1


            if songcounter==1:
                print id3r.dump()



        except IOError, message:
            print "ID TAG ERROR: getIDTags(): IOERROR:", message

    def getOSDir(self,addpath, filelist, depth=0):
        try:
            test=os.path.exists(addpath)
            depth=depth+1
            if (test and depth<8):
                #folderlist.append(addpath)
                #folderCounter=len(folderlist)-1
                for fileName in os.listdir (addpath):
                    try:
                        #filelist.append(addpath+"/"+fileName)
                        if fileName.endswith(".mp3") or fileName.endswith(".MP3"):
                            self.addMp3(addpath+"/"+fileName, fileName)
                        #foldermap.append(folderCounter)
                        if os.path.isdir(addpath+"/"+fileName) and not (fileName.find('.')==0) and not (fileName.find("debian")==0) and not (fileName.find('Maps')==0) and not (fileName.find('maps')==0):
                            self.getOSDir(addpath+"/"+fileName,filelist, depth)
                    except OSError, message:
                        print "getOSDir():OSError:", message
        except OSError, message:
            print "getOSDir():OSError:", message


fileProcessor=FileProcessor()
startTime = time.time()
fileProcessor.getOSDir(indexPath, filelist, depth)
fileProcessor.f.close()
print "created "+ str(songcounter) +" songs to turtle file in " + str(time.time()-startTime)+ " seconds."
