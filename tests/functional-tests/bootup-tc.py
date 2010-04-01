#!/usr/bin/env python2.5

# Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA  02110-1301, USA.


import sys,os,dbus
import unittest
import time
import random
import commands
import string
import configuration

TRACKER = 'org.freedesktop.Tracker1'
TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources'
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources"

IMAGES_DEFAULT = configuration.MYDOCS_IMAGES
MUSIC_DEFAULT  = configuration.MYDOCS_MUSIC
VIDEOS_DEFAULT = configuration.MYDOCS_VIDEOS



def files_list(dirName):            
        fileList = []                                              
        for file in os.listdir(dirName):
            dirfile = os.path.join(dirName, file)
            if dirName == IMAGES_DEFAULT :     
                    if os.path.isfile(dirfile):
                      if file.split('.')[1]== 'jpg' :              
                         fileList.append(dirfile)
                          
            elif dirName == MUSIC_DEFAULT :
                    if os.path.isfile(dirfile):
                      if file.split('.')[1]== 'mp3' :
                         fileList.append(dirfile)        
                                                                                                                  
            elif dirName == VIDEOS_DEFAULT :                                                 
                    if os.path.isfile(dirfile):
                      if file.split('.')[1]== 'avi' :
                         fileList.append(dirfile)                                                              
                                                                                                        

        return fileList



class TestUpdate (unittest.TestCase):
        
        def setUp(self):
                bus = dbus.SessionBus()
                tracker = bus.get_object(TRACKER, TRACKER_OBJ)
                self.resources = dbus.Interface (tracker,
                                                 dbus_interface=RESOURCES_IFACE)


        def sparql_update(self,query):
                return self.resources.SparqlUpdate(query)

        def query(self,query):
                return self.resources.SparqlQuery(query)
	
        def check ( self , appcn ):
		pid = commands.getoutput("ps -ef| grep tracker | awk '{print $3}'").split()                      
                if appcn in  pid :                                                  
		   return 1
                else :                                                                                           
		   return 0	

class tracker_daemon(TestUpdate):

	def test_miner_01(self) :
	    appcn = configuration.TRACKER_MINER
	    result=self.check(appcn)
	    self.assert_(result==1,"tracker miner is not running" )

	def test_store_02(self) :
	    appcn = configuration.TRACKER_STORE  
            result=self.check(appcn)
            self.assert_(result==1,"tracker store is not running" )
			
class default_content(TestUpdate):	   
	
	def test_images_01(self) :
	    """
	       1.Check the no.of files in default folder
	       2.Make tracker search for images and check if files are listed from default folders and count them.
	       3.Check if no.of files from default folder is equal to tracker search results
	    """
	    default_Images=files_list(IMAGES_DEFAULT)
	    Images  = commands.getoutput('tracker-search -i -l 10000 | grep /home/user/MyDocs/.images/|wc -l' )
	    self.assert_(len(default_Images)==int(Images) , "Files are not indexed from default folder")


	def test_music_02(self) :
	
	    """
	       1.Check the no.of files in default folder
	       2.Make tracker search for songs and check if files are listed from default folders and count them.
	       3.Check if no.of files from default folder is equal to tracker search results
	    """
	    default_music=files_list(MUSIC_DEFAULT)
	    Music  = commands.getoutput('tracker-search -u -l 10000 | grep /home/user/MyDocs/.sounds/|wc -l' )
	    self.assert_(len(default_music)==int(Music) , "Files are not indexed from default folder")


	def test_Video_03(self) :
	
	    """
	       1.Check the no.of files in default folder
	       2.Make tracker search for videos and check if files are listed from default folders and count them.
	       3.Check if no.of files from default folder is equal to tracker search results
	    """

	    default_videos=files_list(VIDEOS_DEFAULT)
	    Videos  = commands.getoutput('tracker-search -v -l 10000 | grep /home/user/MyDocs/.videos/|wc -l' )
	    self.assert_(len(default_videos)==int(Videos) , "Files are not indexed from default folder")




if __name__ == "__main__":
	unittest.main()


