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
    print "Usage: python musicToTurtle.py <path-to-index>."
    sys.exit (1)


songcounter=0

filelist=[]
folderlist=[]
foldermap=[]
depth=0
artist_UID = {}
class FileProcessor:

    def __init__(self):
        self.f=open("./songlist.ttl", 'w' )

        self.f.write("@prefix nco:   <http://www.semanticdesktop.org/ontologies/2007/03/22/nco#>.\n")
        self.f.write("@prefix rdfs:  <http://www.w3.org/2000/01/rdf-schema#>.\n")
        self.f.write("@prefix nrl:   <http://www.semanticdesktop.org/ontologies/2007/08/15/nrl#>.\n")
        self.f.write("@prefix nid3:   <http://www.semanticdesktop.org/ontologies/2007/05/10/nid3#>.\n")
        self.f.write("@prefix nao:   <http://www.semanticdesktop.org/ontologies/2007/08/15/nao#>.\n")
        self.f.write("@prefix nfo:   <http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#>.\n")
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

            #UID=str(random.randint(0, sys.maxint))
            UID = ""
            if not artist_UID.has_key(artist):
                #print " The new  artist is "+artist
                UID = str(random.randint(0, sys.maxint))
                artist_UID[artist] = UID
                self.f.write('<urn:uuid:'+UID+'> a nco:Contact; \n')
                self.f.write('\tnco:fullname "'+artist+'".\n')
            else :
                #print 'Artist exists ' + artist
                UID = artist_UID[artist]

            self.f.write('<file://'+urllib.pathname2url(fullpath)+'> a nid3:ID3Audio,nfo:FileDataObject;\n')
            if len(fileName)>0: self.f.write('\tnfo:fileName "'+fileName+'";\n')
            if len(modified)>0: self.f.write('\tnfo:fileLastModified "'+modified+'" ;\n')
            if len(created)>0: self.f.write('\tnfo:fileCreated "'+created+'";\n')
            self.f.write('\tnfo:fileSize '+str(size)+';\n')
            if len(album)>0: self.f.write('\tnid3:albumTitle "'+album+'";\n')
            if len(year)>0: self.f.write('\tnid3:recordingYear '+str(year)+';\n')
            if len(song)>0: self.f.write('\tnid3:title "'+song+'";\n')
            if len(trackstr)>0: self.f.write('\tnid3:trackNumber "'+trackstr+'";\n')
            if len(partOfSet)>0: self.f.write('\tnid3:partOfSet "'+partOfSet+'";\n')

            if len(genre)>0: self.f.write('\tnid3:contentType "'+genre+'";\n')
            if len(comment)>0: self.f.write('\tnid3:comments "'+comment+'";\n')
            if length>0: self.f.write('\tnid3:length '+str(length)+';\n')
            if len(UID)>0: self.f.write('\tnid3:leadArtist <urn:uuid:'+UID+'>.\n\n')


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


