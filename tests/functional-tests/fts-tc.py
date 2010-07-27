#!/usr/bin/env python
#-*- coding: latin-1 -*-


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
import configuration
from dbus.mainloop.glib import DBusGMainLoop
import gobject
import shutil
from subprocess import Popen,PIPE

TRACKER = 'org.freedesktop.Tracker1'

TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources'
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources"


MINER="org.freedesktop.Tracker1.Miner.Files"
MINER_OBJ="/org/freedesktop/Tracker1/Miner/Files"
MINER_IFACE="org.freedesktop.Tracker1.Miner"

TEST_IMAGE = "test-image-1.jpg"
TEST_MUSIC = "tracker-mp3-test.mp3"
TEST_VIDEO = "test-video.mp4"
TEST_TEXT =  "test-text-01.txt"
TEST_TEXT_02 = "test-text-02.txt"
TEST_TEXT_03 = "test-text-03.txt"
TEXT_DB = "/home/user/.cache/tracker/fulltext.db"

MOUNT_DUMMY = '/root/dummy/'                                       
MYDOCS = configuration.MYDOCS                                     
MOUNT_PARTITION = '/dev/mmcblk0p1' 

"""create two test directories in miner monitored path  """
target = configuration.check_target()

if target == configuration.MAEMO6_HW:
    """target is device """
    TEST_DIR_1 = configuration.MYDOCS + "tracker_test_op_1"
    TEST_DIR_2 = configuration.MYDOCS + "tracker_test_op_2"
    """create a test directory in miner un-monitored path  """
    #TEST_DIR_3 = configuration.TEST_DIR + "tracker_test_op_3"
    TEST_DIR_3 = configuration.MYDOCS + "core-dumps/" + "tracker_test_op_3"
    SRC_IMAGE_DIR = configuration.TEST_DATA_IMAGES
    SRC_MUSIC_DIR = configuration.TEST_DATA_MUSIC
    SRC_VIDEO_DIR = configuration.TEST_DATA_VIDEO
    SRC_TEXT_DIR  = configuration.TEST_DATA_TEXT
    MYDOCS_SUB =  configuration.MYDOCS + 's1/s2/s3/s4/s5/'

elif target == configuration.DESKTOP:
    """target is DESKTOP """
    TEST_DIR_1 = os.path.expanduser("~") + '/' + "tracker_test_op_1"
    TEST_DIR_2 = os.path.expanduser("~") + '/' + "tracker_test_op_2"
    TEST_DIR_3 = os.path.expanduser("~") + '/' + "core-dumps/" + "tracker_test_op_3"
    SRC_IMAGE_DIR = configuration.VCS_TEST_DATA_IMAGES
    SRC_MUSIC_DIR = configuration.VCS_TEST_DATA_MUSIC
    SRC_VIDEO_DIR = configuration.VCS_TEST_DATA_VIDEO
    MYDOCS_SUB =  os.path.expanduser("~") + 's1/s2/s3/s4/s5/'

commands.getoutput('mkdir ' + TEST_DIR_1)           
                                                                         
"""creating hidden directory """                                                                                             
HIDDEN_DIR = configuration.MYDOCS + '.test_hidden' 
commands.getoutput('mkdir -p  ' + HIDDEN_DIR)       
                                                                                                                             
tdcpy = configuration.TDCopy()                                                                                               
tdcpy.set_test_data(target)   

"""creating text file"""

print SRC_TEXT_DIR+TEST_TEXT
f1=open(SRC_TEXT_DIR+TEST_TEXT,'w')

lines = "The Content framework subsystem provides data and metadata storage and retrieval for the platform. Its stack contains\
    * A store of information (tracker) based on the triplet-store concept including a specific query language (SparQL, W3C standard)\
    * A convenience library (libQtTracker) with common functionality for all client applications  \
    * A set of widgets for tagging and content browsing (subject of its own architecture document) \
    * A daemon to create thumbnails (Tumbler) \
    * A library (libthumbnailer) for a convenient access to the thumbnail daemon  \
    "
f1.write(lines)
f1.close()

class TestUpdate (unittest.TestCase):

    def setUp(self):
        bus = dbus.SessionBus()
        tracker = bus.get_object(TRACKER, TRACKER_OBJ)
        self.resources = dbus.Interface (tracker,
                                         dbus_interface=RESOURCES_IFACE)

        miner_obj= bus.get_object(MINER,MINER_OBJ)
        self.miner=dbus.Interface (miner_obj,dbus_interface=MINER_IFACE)


        self.loop = gobject.MainLoop()
        self.dbus_loop = DBusGMainLoop(set_as_default=True)
        self.bus = dbus.SessionBus (self.dbus_loop)

        self.bus.add_signal_receiver (self.miner_processing_cb,
                                      signal_name="Progress",
                                      dbus_interface=MINER_IFACE,
                                      path=MINER_OBJ)

    def miner_processing_cb (self,status,handle):
        print "GOT PROGRESS FROM MINER"

        if (status == "Processing Files") :
            print "Miner started"
        elif (status == "Idle" ):
            """if the string is "Idle" quit the loop """
            print "Miner Idle"
            self.loop.quit()
        else :
            print "No specific Signal"

    def wait_for_fileop (self, cmd, src, dst=''):
        if (cmd == "rm"):
            os.remove(src)
        elif (cmd == "cp"):
            shutil.copy2(src, dst)
        else:
            shutil.move(src,dst)
        self.loop.run()

    def edit_text (self, file,word) :                                                                          
        test_file =  file                                             
        f=open(test_file,"w")                                                                             
        f.writelines(word)                                                                               
        f.close()    

class basic (TestUpdate) :

      def test_text_01 (self) :
	
	""" To check if tracker search for a long word gives results """

        file_path =  configuration.MYDOCS + TEST_TEXT
                    
        """copy the test files """          
	time.sleep(1)
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path)

	word = "fsfsfsdfskfweeqrewqkmnbbvkdasdjefjewriqjfnc"

	self.edit_text(file_path,word)

	self.loop.run()

	result=commands.getoutput ('tracker-search  ' + word + '|grep  '+file_path+ '|wc -l ')
	print result
	self.assert_(result=='1','search for the word is not giving results')
	os.remove(file_path)

      def test_text_02 (self) :

 	""" To check if tracker search for a word gives results """

        file_path =  configuration.MYDOCS + TEST_TEXT
                    
        """copy the test files """          
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path)
	word = "this is a meego file "
	search_word = "meego"

	self.edit_text(file_path,word)
	self.loop.run()

	result=commands.getoutput ('tracker-search  ' + search_word+ '|grep  '+file_path+ '|wc -l ' )
	print result
	self.assert_(result=='1','search for the word is not giving results')

	os.remove (file_path)

      def test_text_03 (self) :
	
	""" To check if tracker search for a non existing word gives results """

        file_path =  configuration.MYDOCS + TEST_TEXT
                    
        """copy the test files """          
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path)
	word = "trick "
	search_word = "trikc"

	self.edit_text(file_path,word)
	self.loop.run()

	result=commands.getoutput ('tracker-search  ' + search_word + '|grep  '+file_path+ '|wc -l ')
	print result
	self.assert_(result=='0','search for the non existing  word is giving results')

	os.remove (file_path)

      def test_text_04 (self) :

	""" To check if tracker search for a word gives results (File contains same word multiple times)"""  

        file_path =  configuration.MYDOCS + TEST_TEXT
                    
        """copy the test files """          
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path)
	sentence= "this is a crazy day. i m feeling crazy. rnt u crazy. everyone is crazy."
	search_word = "crazy"

	self.edit_text(file_path,sentence)
	self.loop.run()

	result=commands.getoutput ('tracker-search  ' + search_word + '|grep  '+file_path+ '|wc -l ')
	print result
	self.assert_(result=='1','search for the word  not giving results')

	os.remove (file_path)

      def test_text_05 (self) :

	""" To check if tracker search for sentence gives results """  

        file_path =  configuration.MYDOCS + TEST_TEXT
                    
        """copy the test files """          
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path)
	sentence= " 'this is a lazy fox. '"

	self.edit_text(file_path,sentence)
	self.loop.run()

	result=commands.getoutput ('tracker-search  ' +sentence+ '|grep  '+file_path+ '|wc -l ')
	print result
	self.assert_(result=='1','search for the sentence is not giving results')

	os.remove (file_path)

    
      def test_text_06 (self) :

	""" To check if tracker search for part of sentenece gives results """  

        file_path =  configuration.MYDOCS + TEST_TEXT
                    
        """copy the test files """          
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path)
	sentence= " 'this is a lazy fox. '"
	search_sentence =  " 'this is a lazy  '"

	self.edit_text(file_path,sentence)
	self.loop.run()

	result=commands.getoutput ('tracker-search  ' + search_sentence+ '|grep  '+file_path+ '|wc -l ')
	print result
	self.assert_(result=='1','search for the sentence is not giving results')

	os.remove (file_path)
    

      def test_text_07 (self) :

	""" To check if tracker search for  sentenece gives results """	
        file_path =  configuration.MYDOCS + TEST_TEXT
                    
        """copy the test files """          
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path)
	sentence= " 'summer.time '"

	self.edit_text(file_path,sentence)
	self.loop.run()

	result=commands.getoutput ('tracker-search  ' + sentence+ '|grep  '+file_path+ '|wc -l ')
	print result
	self.assert_(result=='1','search for the sentence is not giving results')

	os.remove (file_path)


      def test_text_08 (self) :

	""" To check if tracker search for  sentenece gives results """ 

	file_path =  configuration.MYDOCS + TEST_TEXT                   
                                                                        
        """copy the test files """                                      
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path) 
        sentence= " 'summer.time '"                                     
        search_word = '.'                                           

        self.edit_text(file_path,sentence)                              
        self.loop.run()                                                 

        result=commands.getoutput ('tracker-search  ' + search_word+ '|grep  '+file_path+ '|wc -l ')    
        print result                                                    
	self.assert_(result=='0','search for the word is not giving results')

        os.remove (file_path)

      def test_text_09 (self) :

	""" To check if tracker search for a word (combination of alphabets and numbers)  gives results """ 

	file_path =  configuration.MYDOCS + TEST_TEXT                   
                                                                        
        """copy the test files """                                      
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path) 
        word = "abc123"                                     

        self.edit_text(file_path,word)                              
        self.loop.run()                                                 

        result=commands.getoutput ('tracker-search  ' + word+ '|grep  '+file_path+ '|wc -l ')    
        print result                                                    
	self.assert_(result=='1','search for the word is not giving results')

        os.remove (file_path)

      def test_text_10 (self) :
	""" To check if tracker search for a number (from a combination of alphabets and numbers)  gives results """

	file_path =  configuration.MYDOCS + TEST_TEXT                   
                                                                        
        """copy the test files """                                      
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path) 
        sentence = "abc 123"                                     
	search_word = "123"

        self.edit_text(file_path,search_word)                              
        self.loop.run()                                                 

        result=commands.getoutput ('tracker-search  ' + search_word+ '|grep  '+file_path+ '|wc -l ')    
        print result                                                    
	self.assert_(result=='0','search for the word is not giving results')

        os.remove (file_path)

      def test_text_12 (self) :

	""" To check if tracker-search for a word(file which contains this file is removed) gives result"""

	file_path =  configuration.MYDOCS + TEST_TEXT                   
                                                                        
        """copy the test files """                                      
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path) 
	word = "abc"
        self.edit_text(file_path,word)                              
        self.loop.run()                                                 

        result=commands.getoutput ('tracker-search  ' + word+ '|grep  '+file_path+ '|wc -l ')    
        print result                                                    
	self.assert_(result=='1','search for the word is not giving results')

        self.wait_for_fileop('rm' , file_path)

        result=commands.getoutput ('tracker-search  ' + word+ '|grep  '+file_path+ '|wc -l ')    
	
        result1=commands.getoutput ('tracker-search  ' + word+ '|grep  '+file_path)    
	print result1
        print result                                                    
	self.assert_(result=='0','search for the non existing files giving results')

      def test_text_13 (self) :                                                                                              
                                                                                                                             
        """ To check if tracker-search for a word in different text files with similar 3 letter words and search for the word gives result """
                                                                                                                             
        file_path =  configuration.MYDOCS + TEST_TEXT                                                                        
        file_path_02 =  configuration.MYDOCS + TEST_TEXT_02                                                                  
        file_path_03 =  configuration.MYDOCS + TEST_TEXT_03                                                                  
                                                                                                                             
        """copy the test files """                                                                                           

        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path)                                                      
        sentence= " 'feet '"                                                                                                 
        self.edit_text(file_path,sentence)                                                                                   
        self.loop.run()                                                                                                      
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path_02)                                                   
        sentence= " 'feel '"                                                                                                 
        self.edit_text(file_path_02,sentence)                                                                                
        self.loop.run()                                                                                                      
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path_03)                                                   
        sentence= " 'fee '"                                                                                                  
        self.edit_text(file_path_03,sentence)                                                                                
        self.loop.run()                                                                                                      
                                                                                                                             
        search_word = 'feet'                                                                                                 
        result=commands.getoutput ('tracker-search  ' + search_word+ '|grep  '+file_path+ '|wc -l ')                         
        print result                                                                                                         
        self.assert_(result=='1','search for the word is not giving results')                                                
                                                                                                                             
        os.remove (file_path)                                                                                                
        os.remove (file_path_02)                                                                                             
        os.remove (file_path_03)      

      def test_text_14 (self) :                                                                                              
                                                                                                                             
        """ To check if tracker-search for a word in unwatched directory and gives result"""                                 
                                                                                                                             
        file_path =  "/root/" + TEST_TEXT                                                                                    
                                                                                                                             
        """copy the test files """                                                                                           
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path)                                                      
        word = "framework"                                                                                                   
                                                                                                                             
        result=commands.getoutput ('tracker-search  ' + word+ '|grep  '+file_path+ '|wc -l ')                                
        print result                                                                                                         
        self.assert_(result=='0','search for the word is not giving results')                                                
                                                                                                                             
        os.remove (file_path)                                                                                                
                                                                                                                             
      def test_text_15 (self) :                                                                                              
                                                                                                                             
        """ To check if tracker-search for a word(file which is copied from no watch directories to watched directories) gives results """
                                                                                                                             
        FILE_NAME =  "/root/" + TEST_TEXT                                                                                    
        file_path =  configuration.MYDOCS + TEST_TEXT                                                                        
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path)                                                      
        word= "The Content framework subsystem provides data"                                                                
                                                                                                                             
        self.edit_text(FILE_NAME,word)                                                                                       
                                                                                                                             
        """copy the test files """                                                                                           
        self.wait_for_fileop('cp', FILE_NAME, file_path)                                                                     
                                                                                                                             
        word = "framework"                                                                                                   
                                                                                                                             
        result=commands.getoutput ('tracker-search  ' + word+ '|grep  '+file_path+ '|wc -l ')                                
        print result                                                                                                         
        self.assert_(result=='1','search for the word is giving results')                                                    
                                                                                                                             
        os.remove (file_path)                               

      def test_text_16 (self) :                                                                                              
                                                                                                                             
        """ To check if tracker-search for a word(file which is in hidden directory) gives result"""                         
        file_path =  HIDDEN_DIR+TEST_TEXT                                                                                   
                                                                                                                             
        """copy the test files """                                                                                           
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT , file_path)                                                     
        word = "framework"                                                                                                   
                                                                                                                             
        result=commands.getoutput ('tracker-search  ' + word+ '|grep  '+file_path+ '|wc -l ')                                
        print result                                                                                                         
        self.assert_(result=='0','search for the word is giving results')                                                    
                                                                                                                             
        os.remove (file_path)       


class stopwords (TestUpdate):	
     
      def test_sw_01 (self) :
	file_path =  configuration.MYDOCS + TEST_TEXT                   
                                                                        
        """copy the test files """                                      
	test_file='/usr/share/tracker/languages/stopwords.en'
	f1=open(test_file,'r')
	lines = f1.readlines()
	f1.close()
	list = []
	for word in lines:
		result=Popen(['tracker-search',word],stdout=PIPE).stdout.read().split()
		if result[1] == '1':
			list.append(word)
	self.assert_(len(list) == 0 , 'tracker search is giving results for stopwords %s '%list)

      def test_sw_02 (self) :
	
        word= "AND"                                     
	result=Popen(['tracker-search',word],stdout=PIPE).stdout.read().split()             
        self.assert_(result[1] == '0' , 'tracker search is giving results for stopwords')

class db_text ( TestUpdate ) :

      def test_db_01 (self):
	file_path =  configuration.MYDOCS + TEST_TEXT		
	self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path) 
        word = "summer"                                     
        self.edit_text(file_path,word)
	time.sleep(2)
	result=commands.getoutput ('tracker-search  ' + word+ '|grep  '+file_path+ '|wc -l ')                         
        print result                                                                                                         
        self.assert_(result=='1','search for the word is not giving results')   
	commands.getoutput('cp  '+SRC_IMAGE_DIR+TEST_IMAGE+ '  '+TEXT_DB)
	time.sleep(1)
	result=commands.getoutput ('tracker-search  ' + word+ '|grep  '+file_path+ '|wc -l ')                         
        print result                                                                                                         
        self.assert_(result=='1','search for the word is not giving results')   




class msm (TestUpdate) :
	
      def test_msm_01 (self) :

	""" To check if tracker search gives results for the word search which is copied in mass storage mode """
	
	commands.getoutput ('umount  ' + MOUNT_PARTITION )
	commands.getoutput ('mount -t vfat -o rw ' +MOUNT_PARTITION+'  '+MOUNT_DUMMY)

	dummy_path =  MOUNT_DUMMY + TEST_TEXT		
	file_path  =   configuration.MYDOCS+TEST_TEXT 
	commands.getoutput('cp   '+SRC_TEXT_DIR + TEST_TEXT +'  '+ dummy_path) 

	commands.getoutput ('umount  ' + MOUNT_PARTITION )
	commands.getoutput ('mount -t vfat -o rw ' +MOUNT_PARTITION+'  '+MYDOCS)
	time.sleep(10)
        search_word = "information"                                     
	result=commands.getoutput ('tracker-search  ' + search_word+ '|grep  '+file_path+ '|wc -l ')                         
        print result                                                                                                         
        self.assert_(result=='1','search for the word is not giving results')   

class specialchar (TestUpdate) :
	
      def test_sc_01 (self):
     	""" To check if tracker search for non English characters """

        file_path =  configuration.MYDOCS + TEST_TEXT

        """copy the test files """
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path)
        sentence= " 'andaÃ¼bÃc' Ã"

        self.edit_text(file_path,sentence)
        self.loop.run()

        result=commands.getoutput ('tracker-search  ' +sentence+ '|grep  '+file_path+ '|wc -l ')
        print result
        self.assert_(result=='1','search for the sentence is not giving results')

        os.remove (file_path)

      def test_cs_02 (self):

        file_path =  configuration.MYDOCS + TEST_TEXT

        """copy the test files """
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path)

	sentence = "выйшщхжюб"

        self.edit_text(file_path,sentence)

        self.loop.run()

        result=commands.getoutput ('tracker-search  ' +sentence+ '|grep  '+file_path+ '|wc -l ')
        print result
        self.assert_(result=='1','search for the sentence is not giving results')

        os.remove (file_path)

      def test_cs_03 (self) :
	 
	""" To check if tracker search for non English characters """

        file_path =  configuration.MYDOCS + TEST_TEXT

        """copy the test files """
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path)
        sentence = " 'ÐºÑ<80>Ð°Ñ<81>Ð' "

        self.edit_text(file_path,sentence)
        self.loop.run()

        result=commands.getoutput ('tracker-search  ' +sentence+ '|grep  '+file_path+ '|wc -l ')
        print result
        self.assert_(result=='1','search for the sentence is not giving results')

        os.remove (file_path)

      def test_cs_04 (self) :

	""" To check if tracker search for non English  accented characters """

        file_path =  configuration.MYDOCS + TEST_TEXT

        """copy the test files """
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path)
        sentence = " 'aÃ¼Ã©Ã¢bc Ã¤Ã Ã§ xÃªÃ«Ã¨Ã¯yz Ã¼Ã©Ã¢Ã¤Ã Ã§ÃªÃ«Ã¨Ã¯ and aÃ¼bÃªcÃ«dÃ§eÃ¯' "

        self.edit_text(file_path,sentence)
        self.loop.run()

        result=commands.getoutput ('tracker-search  ' +sentence+ '|grep  '+file_path+ '|wc -l ')
        print result
        self.assert_(result=='1','search for the sentence is not giving results')

        os.remove (file_path)

      def test_cs_05 (self) :

	""" To check if tracker search for combination of English characters and accented characters"""

        file_path =  configuration.MYDOCS + TEST_TEXT

        """copy the test files """
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path)
        sentence = " 'beautiful and xÃªÃ«Ã¨Ã¯yz Ã¼Ã©Ã¢Ã¤Ã Ã§ÃªÃ«Ã¨Ã¯ and aÃ¼bÃªcÃ«dÃ§eÃ¯' "

        self.edit_text(file_path,sentence)
        self.loop.run()

        result=commands.getoutput ('tracker-search  ' +sentence+ '|grep  '+file_path+ '|wc -l ')
        print result
        self.assert_(result=='1','search for the sentence is not giving results')

        os.remove (file_path)
	  
      def test_cs_06 (self):
	  
	""" To check if tracker search for normalisation """

        file_path =  configuration.MYDOCS + TEST_TEXT

        """copy the test files """
        self.wait_for_fileop('cp', SRC_TEXT_DIR + TEST_TEXT, file_path)
	sentence = "école"
        word = " 'ecole' "

        self.edit_text(file_path,sentence)
        self.loop.run()

        result=commands.getoutput ('tracker-search  ' +word+ '|grep  '+file_path+ '|wc -l ')
        print result
        self.assert_(result=='1','search for the sentence is not giving results')

        os.remove (file_path)
	
class applications (TestUpdate) :

	def test_app_Images (self) :

	    result= commands.getoutput ('tracker-search -i ' + TEST_IMAGE+ '|wc -l')
	    self.assert_(result!=0 , 'tracker search for images is not giving results')


	def test_app_Music (self) :

	    result= commands.getoutput ('tracker-search -m ' + TEST_MUSIC+ '|wc -l')
	    self.assert_(result!=0 , 'tracker search for music is not giving results')

	def test_app_Vidoes (self) :

	    result= commands.getoutput ('tracker-search -v ' + TEST_VIDEO+ '|wc -l')
	    self.assert_(result!=0 , 'tracker search for Videos is not giving results')


	def test_app_music_albums (self) :

	    result= commands.getoutput ('tracker-search --music-albums SinCos  | wc -l')
	    self.assert_(result!=0 , 'tracker search for music albums is not giving results')

	
	def test_app_music_artists (self) :

	    result= commands.getoutput ('tracker-search --music-artists AbBaby  | wc -l')
	    self.assert_(result!=0 , 'tracker search for music artists is not giving results')

	def test_app_folders (self) :

	    result= commands.getoutput ('tracker-search -s '+TEST_DIR_1 + '| wc -l')
	    self.assert_(result!=0 , 'tracker search for folders is not giving results')

	
	def test_app_email (self) :

	    INSERT = """ INSERT {<qmf://fenix.nokia.com/email#3333> a nmo:Email ;
			 nmo:receivedDate '2010-05-24T15:17:26Z' ;
 			 nmo:messageSubject 'searching_for_Email' } 
                      """
	    self.resources_new.SparqlUpdate (INSERT)
	    
	    result = commands.getoutput ('tracker-search -ea searching_for_Email |wc -l ')
	    self.assert_(result!=0 , 'tracker search for files is not giving results')

	def test_app_email (self) :

	    INSERT = """ INSERT {<qmf://fenix.nokia.com/email#3333> a nmo:Email ;
			 nmo:receivedDate '2010-05-24T15:17:26Z' ;
 			 nmo:messageSubject 'searching_for_Email' } 
                      """
	    #self.resources.SparqlUpdate (INSERT)
            self.resources.SparqlUpdate  (INSERT)
	    
	    result = commands.getoutput ('tracker-search -ea searching_for_Email |wc -l ')
	    self.assert_(result!=0 , 'tracker search for files is not giving results')


if __name__ == "__main__":  
	unittest.main()
	"""
	basic_tcs_list=unittest.TestLoader().getTestCaseNames(basic)       
        basic_testsuite=unittest.TestSuite(map(basic, basic_tcs_list))

	db_text_tcs_list=unittest.TestLoader().getTestCaseNames(db_text)       
        db_text_testsuite=unittest.TestSuite(map(db_text, db_text_tcs_list))
	msm_tcs_list=unittest.TestLoader().getTestCaseNames(msm)       
        msm_testsuite=unittest.TestSuite(map(msm, msm_tcs_list))

	stopwords_tcs_list=unittest.TestLoader().getTestCaseNames(stopwords)       
        stopwords_testsuite=unittest.TestSuite(map(stopwords, stopwords_tcs_list))

	applications_tcs_list=unittest.TestLoader().getTestCaseNames(applications)       
        applications_testsuite=unittest.TestSuite(map(applications, applications_tcs_list))

	specialchar_tcs_list=unittest.TestLoader().getTestCaseNames(specialchar)       
        specialchar_testsuite=unittest.TestSuite(map(specialchar, specialchar_tcs_list))

        all_testsuites = unittest.TestSuite(specialchar_testsuite)
        unittest.TextTestRunner(verbosity=2).run(all_testsuites)
	"""
