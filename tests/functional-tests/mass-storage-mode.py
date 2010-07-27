#/bin/env python

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

TRACKER = 'org.freedesktop.Tracker1'
TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources/Classes'
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources.Class"

MINER="org.freedesktop.Tracker1.Miner.Files"
MINER_OBJ="/org/freedesktop/Tracker1/Miner/Files"
MINER_IFACE="org.freedesktop.Tracker1.Miner"



TEST_IMAGE = "test-image-1.jpg"
TEST_MUSIC = "tracker-mp3-test.mp3"
TEST_VIDEO = "test-video.mp4"
TEST_TEXT =  "test-text-01.txt"

MOUNT_DUMMY = '/root/dummy/'
MYDOCS = configuration.MYDOCS
MOUNT_PARTITION = '/dev/mmcblk0p1'

SUB_FOLDER_DUMMY =  MOUNT_DUMMY + '/' + 's1/s2/s3/s4/s5/'
SUB_FOLDER_MYDOCS =  MYDOCS + 's1/s2/s3/s4/s5/'


DUMMY_DIR = '/root/dummy/TEST_DIR/'
MYDOCS_DIR = '/home/user/MyDocs/TEST_DIR/'

RENAME_IMAGE = 'rename_image.jpg'
RENAME_MUSIC = 'rename_song.mp3'
RENAME_VIDEO = 'rename_video.mp4'
"""create two test directories in miner monitored path  """
target = configuration.check_target()

def check_mount() :
	if not os.path.exists (MOUNT_DUMMY) :
		commands.getoutput ('mkdir  ' + MOUNT_DUMMY)

	commands.getoutput ('umount ' + MOUNT_PARTITION )

	if commands.getoutput('df').find('/home/user/MyDocs') == -1  and commands.getoutput('df').find('/dev/mmcblk0p1') == -1 :
		print 'in else if'
 		commands.getoutput ('mount  ' + MOUNT_PARTITION + '  ' + MYDOCS )
	else :
		 print "in else else"
		 commands.getoutput ('umount ' + MOUNT_PARTITION )
		 commands.getoutput ('mount ' + MOUNT_PARTITION + '  ' + MYDOCS )


if target == configuration.MAEMO6_HW:
	"""target is device """
	SRC_IMAGE_DIR = configuration.TEST_DATA_IMAGES
	SRC_MUSIC_DIR = configuration.TEST_DATA_MUSIC
	SRC_VIDEO_DIR = configuration.TEST_DATA_VIDEO
	MYDOCS_SUB =  configuration.MYDOCS + 's1/s2/s3/s4/s5/'


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
        if (status == "Idle" ):
            print "Miner is in idle status "
	    self.loop.quit ()
        else :
            print "Miner not in Idle "

    def wait_for_fileop (self, cmd, src, dst=''):

        if (cmd == "rm"):
            os.remove(src)
        elif (cmd == "cp"):
            shutil.copy2(src, dst)
        else:
            shutil.move(src,dst)
        self.loop.run()

    def edit_text (self, file) :
	test_file = MOUNT_DUMMY + file
	f=open(test_file,"w")
	lines = "Editing this file to test massstorage mode"
	f.writelines(lines)
	f.close()

""" copy in mass storage mode test cases """
class copy (TestUpdate):

    def test_image_01 (self):

        """ To check if tracker indexes image file copied in massstorage mode """

        file_path = MYDOCS + TEST_IMAGE

        check_mount()
        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount -t vfat -o rw  '  + MOUNT_PARTITION + ' ' + MOUNT_DUMMY)
	  

        """copy the test file """
        shutil.copy2(SRC_IMAGE_DIR + TEST_IMAGE, MOUNT_DUMMY)

        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount -t vfat -o rw '  + MOUNT_PARTITION + ' ' + MYDOCS)
	  

        print commands.getoutput ( 'ls ' + MYDOCS)

        self.loop.run ()

        """ verify if miner indexed these file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path + ' | wc -l')
        print result
        self.assert_(result == '1' , 'copied image file is not shown as indexed')

        os.remove(file_path)

    def test_music_02 (self):

        """ To check if tracker indexes audio file copied in massstorage mode """

        file_path = MYDOCS + TEST_MUSIC

        check_mount()
        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount -t vfat -o rw '  + MOUNT_PARTITION + ' ' + MOUNT_DUMMY)
	  
        """copy the test files """
        shutil.copy2(SRC_MUSIC_DIR + TEST_MUSIC, MOUNT_DUMMY)

        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount  -t vfat -o rw  '  + MOUNT_PARTITION + ' ' + MYDOCS)
	  
        print commands.getoutput ( 'ls ' + MYDOCS)

        self.loop.run ()

        """ verify if miner indexed these file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path + ' | wc -l')
        print result
        self.assert_(result == '1' , 'copied music file is not shown as indexed')

        os.remove(file_path)

    def test_video_03 (self):

        """ To check if tracker indexes video file copied in massstorage mode """

        file_path = MYDOCS + TEST_VIDEO

        check_mount()
        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount -t vfat -o rw'  + MOUNT_PARTITION + ' ' + MOUNT_DUMMY)
	  
        """copy the test files """
        shutil.copy2(SRC_VIDEO_DIR + TEST_VIDEO, MOUNT_DUMMY)


        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount '  + MOUNT_PARTITION + ' ' + MYDOCS)
	  

        print commands.getoutput ( 'ls ' + MYDOCS)

        self.loop.run ()

        """ verify if miner indexed these file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path + ' | wc -l')
	print result
        self.assert_(result == '1' , 'copied video file is not shown as indexed')

        os.remove(file_path)


""" move in mass storage mode"""
class move (TestUpdate):                                                     
                                                            
                                                            
    def test_image_01 (self):                               
                                                               
        """ To check if tracker indexes moved image file in massstorage mode """      
                                                                                
        file_path_dst =  MYDOCS_DIR + TEST_IMAGE                                
        file_path_src =  MYDOCS + TEST_IMAGE                
                                                                                
        check_mount()                                                           
                                                                                                              
        """copy the test files """                                              
        self.wait_for_fileop('cp', SRC_IMAGE_DIR + TEST_IMAGE, file_path_src)      
                                                                                
        result =  commands.getoutput(' tracker-search -i -l 10000 | grep  ' + file_path_src + ' |wc -l' )      
        self.assert_(result == '1' , 'copied image file is not shown as indexed')                                     
                                                                                                         
        commands.getoutput ( 'umount ' + MOUNT_PARTITION )                       
                                                                                                         
        commands.getoutput ( 'mount '  + MOUNT_PARTITION + ' ' + MOUNT_DUMMY)                            
                                                                                                         
        shutil.move( MOUNT_DUMMY + TEST_IMAGE , DUMMY_DIR)                                               
                                                                                                         
        commands.getoutput ( 'umount ' + MOUNT_PARTITION )                                               
                                                                                 
        commands.getoutput ( 'mount -t vfat -o rw '  + MOUNT_PARTITION + ' ' + MYDOCS)                   
                                                                                                         
        print commands.getoutput ( 'ls ' + MYDOCS)                                                       
        self.loop.run()                                                                                  
                                                                                                         
        """ verify if miner indexed these file.  """                                                     
        result =  commands.getoutput(' tracker-search -i -l 10000 | grep  ' + file_path_dst + ' |wc -l' )
                                                                                                         
        self.assert_(result == '1'  , 'moved file is not listed in tracker search')                      
        result1 = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path_src + ' | wc -l')
        self.assert_(result == '1' and result1 == '0' , 'Both the  original and moved files are listed in tracker search')
                                                                                                                          
        os.remove(file_path_dst)                                                                                          
                                                                                                               
                                                                                                                          
    def test_music_01 (self):                                                                                             
                                                                                                                          
        """ To check if tracker indexes moved audio files in massstorage mode """                                         
                                                                                        

 	file_path_dst =  MYDOCS_DIR + TEST_MUSIC                                                                          
        file_path_src =  MYDOCS + TEST_MUSIC                                                                              
                                                                                                                          
        check_mount()                                                                                                     
                                                                                                                          
        """copy the test files """                                                                                        
        self.wait_for_fileop('cp', SRC_MUSIC_DIR + TEST_MUSIC, file_path_src)                                             
                                                                                                                          
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_src + ' | wc -l')            
        self.assert_(result == '1' , 'copied music file is not shown as indexed')                                         
                                                                                                                          
        commands.getoutput ( 'umount ' + MOUNT_PARTITION )                                                                
                                                                                                                          
        commands.getoutput ( 'mount '  + MOUNT_PARTITION + ' ' + MOUNT_DUMMY)                                             
                                                                                                                          
        shutil.move( MOUNT_DUMMY  + TEST_MUSIC , DUMMY_DIR)                                                               
        commands.getoutput ( 'umount ' + MOUNT_PARTITION )                                                                
                                                                                                                          
        commands.getoutput ( 'mount -t vfat -o rw '  + MOUNT_PARTITION + ' ' + MYDOCS)                                    
                                                                                                                          
        print commands.getoutput ( 'ls ' + MYDOCS)                                                                        
                                                                                                                          
        self.loop.run()                                                                                        
                                                                                                                          
        """ verify if miner indexed these file.  """                                                                      
                                                                                                                          
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_dst + ' | wc -l')            
        print result                                                                                                      
        self.assert_(result == '1' , 'moved music file is not shown as indexed')                                          
                                                                                                                          
        result1 = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_src + ' | wc -l')           
        self.assert_(result == '1' and result1 == '0' , 'Both original and moved files are listed in tracker search ')    
                                                                                                                          
        os.remove(file_path_dst)                                                                                      
                                                                                                                          
                                                                                                                          
    def test_video_01 (self):                                                                                             
                                                                                                                          
        """ To check if tracker indexes moved video files in massstorage mode """                                     
                                                                                                                          
        file_path_dst =  MYDOCS_DIR + TEST_VIDEO                                                                          
        file_path_src =  MYDOCS + TEST_VIDEO                                                                              
                                                                                                                          
        check_mount()                         
	"""copy the test files """                                                                                        
        self.wait_for_fileop('cp', SRC_VIDEO_DIR + TEST_VIDEO, file_path_src)                                             
                                                                                                                          
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_src + ' | wc -l')            
        self.assert_(result == '1' , 'copied video file is not shown as indexed')                                         
                                                                                                                          
        commands.getoutput ( 'umount ' + MOUNT_PARTITION )                                                                
                                                                                                                          
        commands.getoutput ( 'mount '  + MOUNT_PARTITION + ' ' + MOUNT_DUMMY)                                             
                                                                                                                      
        shutil.move(MOUNT_DUMMY + TEST_VIDEO , DUMMY_DIR)                                                                 
        commands.getoutput ( 'umount ' + MOUNT_PARTITION )                                                                
                                                                                                                          
        commands.getoutput ( 'mount -t vfat -o rw '  + MOUNT_PARTITION + ' ' + MYDOCS)                                    
                                                                                                                          
        print commands.getoutput ( 'ls ' + MYDOCS)                                                                        
        self.loop.run()                                                                                                   
        """ verify if miner indexed these file.  """                                                                      
                                                                                                                          
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_dst + ' | wc -l')            
                                                                                                                      
        self.assert_(result == '1' , 'moved file is not listed in tracker search ')                                       
                                                                                                                          
        result1 = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_src + ' | wc -l')           
        self.assert_(result == '1' and result1 == '0' , 'Both original and moved files are listed in tracker search ')    
                                                                                                                      
        os.remove(file_path_dst)                                                                                          
                                                

class rename (TestUpdate):


    def test_image_01 (self):

    	""" To check if tracker indexes renamed image file in massstorage mode """

	file_path_dst =  MYDOCS + RENAME_IMAGE
	file_path_src =  MYDOCS + TEST_IMAGE

        check_mount()

	"""copy the test files """
	self.wait_for_fileop('cp', SRC_IMAGE_DIR + TEST_IMAGE, file_path_src)

	result =  commands.getoutput(' tracker-search -i -l 10000 | grep  ' + file_path_src + ' |wc -l' )
        self.assert_(result == '1' , 'copied image file is not shown as indexed')

        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount '  + MOUNT_PARTITION + ' ' + MOUNT_DUMMY)
	  
	shutil.move( MOUNT_DUMMY + TEST_IMAGE ,  MOUNT_DUMMY+RENAME_IMAGE)

        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount -t vfat -o rw '  + MOUNT_PARTITION + ' ' + MYDOCS)
	  
        print commands.getoutput ( 'ls ' + MYDOCS)
	self.loop.run()

	""" verify if miner indexed these file.  """
	result =  commands.getoutput(' tracker-search -i -l 10000 | grep  ' + file_path_dst + ' |wc -l' )

        self.assert_(result == '1'  , 'renamed file s not listed in tracker search')
	result1 = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path_src + ' | wc -l')
        self.assert_(result == '1' and result1 == '0' , 'Both the  original and renamed files are listed in tracker search')

	os.remove(file_path_dst)


    def test_music_01 (self):

    	""" To check if tracker indexes renamed audio files in massstorage mode """

	file_path_dst =  MYDOCS + RENAME_MUSIC
	file_path_src =  MYDOCS + TEST_MUSIC

        check_mount()

	"""copy the test files """
	self.wait_for_fileop('cp', SRC_MUSIC_DIR + TEST_MUSIC, file_path_src)

        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_src + ' | wc -l')
        self.assert_(result == '1' , 'copied music file is not shown as indexed')

        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount '  + MOUNT_PARTITION + ' ' + MOUNT_DUMMY)
	  
	shutil.move( MOUNT_DUMMY  + TEST_MUSIC , MOUNT_DUMMY+RENAME_MUSIC)
        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount -t vfat -o rw '  + MOUNT_PARTITION + ' ' + MYDOCS)
	  
        print commands.getoutput ( 'ls ' + MYDOCS)

	self.loop.run()

	""" verify if miner indexed these file.  """

        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_dst + ' | wc -l')
        print result
        self.assert_(result == '1' , 'renamed music file is not shown as indexed')

        result1 = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path_src + ' | wc -l')
        self.assert_(result == '1' and result1 == '0' , 'Both original and renamed files are listed in tracker search ')

	os.remove(file_path_dst)


    def test_video_01 (self):

    	""" To check if tracker indexes renamed video files in massstorage mode """

	file_path_dst =  MYDOCS + RENAME_VIDEO
	file_path_src =  MYDOCS + TEST_VIDEO

        check_mount()

	"""copy the test files """
	self.wait_for_fileop('cp', SRC_VIDEO_DIR + TEST_VIDEO, file_path_src)

        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_src + ' | wc -l')
        self.assert_(result == '1' , 'copied video file is not shown as indexed')

        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount '  + MOUNT_PARTITION + ' ' + MOUNT_DUMMY)
	  
	shutil.move(MOUNT_DUMMY + TEST_VIDEO , MOUNT_DUMMY+RENAME_VIDEO)
        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount -t vfat -o rw '  + MOUNT_PARTITION + ' ' + MYDOCS)
	  
        print commands.getoutput ( 'ls ' + MYDOCS)
	self.loop.run()
	""" verify if miner indexed these file.  """

        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_dst + ' | wc -l')

        self.assert_(result == '1' , 'renamed file is not listed in tracker search ')

        result1 = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path_src + ' | wc -l')
        self.assert_(result == '1' and result1 == '0' , 'Both original and renamed files are listed in tracker search ')

	os.remove(file_path_dst)


""" subfolder operations in mass storage mode """
class subfolder (TestUpdate):

    def test_create_01 (self):

        """ To check if tracker indexes image file copied to a
        newly created subfolder in massstorage mode """

        file_path = SUB_FOLDER_MYDOCS + TEST_IMAGE

        check_mount()
        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount '  + MOUNT_PARTITION + ' ' + MOUNT_DUMMY)
	  

        """create a subfolder """
        commands.getoutput('mkdir -p '+ SUB_FOLDER_DUMMY)

        """copy the test file """
        shutil.copy2(SRC_IMAGE_DIR + TEST_IMAGE, SUB_FOLDER_DUMMY)

        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount '  + MOUNT_PARTITION + ' ' + MYDOCS)
	  

        print commands.getoutput ( 'ls ' + SUB_FOLDER_MYDOCS)

        
        self.loop.run ()

        """ verify if miner indexed these file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path + ' | wc -l')
        print result
        self.assert_(result == '1' , 'copied image file is not shown as indexed')

        shutil.rmtree(MYDOCS + 's1')


    def test_delete_02 (self):
	""" To check if tracker un-indexes image file in a

        subfolder if subfolder is deleted in massstorage mode """

        file_path = SUB_FOLDER_MYDOCS + TEST_IMAGE

        """create a subfolder """
        commands.getoutput('mkdir -p '+ SUB_FOLDER_MYDOCS)

        """copy the test file """
        shutil.copy2(SRC_IMAGE_DIR + TEST_IMAGE, SUB_FOLDER_MYDOCS)
        self.loop.run ()

        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path + ' | wc -l')
        print result
        self.assert_(result == '1' , 'copied file is not shown as indexed')

        check_mount()
        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount '  + MOUNT_PARTITION + ' ' + MOUNT_DUMMY)
	  

        shutil.rmtree(MOUNT_DUMMY + '/' + 's1')

        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount '  + MOUNT_PARTITION + ' ' + MYDOCS)
	  

        print commands.getoutput ( 'ls ' + SUB_FOLDER_MYDOCS)

        
        self.loop.run ()

        """ verify if miner un-indexed these file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path + ' | wc -l')
        print result
        self.assert_(result == '0' , 'deleted image file is shown as indexed')


""" delete files in mass storage mode """
class delete (TestUpdate):

    def test_image_01 (self):

        """ To check if tracker indexes image if its deleted in massstorage mode """

        file_path = MYDOCS + TEST_IMAGE

        """copy the test files """
        self.wait_for_fileop('cp', SRC_IMAGE_DIR + TEST_IMAGE, file_path)

        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path + ' | wc -l')
        print result
        self.assert_(result == '1' , 'copied file is not shown as indexed')

        check_mount()
        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount '  + MOUNT_PARTITION + ' ' + MOUNT_DUMMY)
	  
        """remove the test files """
        os.remove(MOUNT_DUMMY + '/' + TEST_IMAGE)


        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount '  + MOUNT_PARTITION + ' ' + MYDOCS)
	  

        print commands.getoutput ( 'ls ' + MYDOCS)

       
        self.loop.run ()

        """ verify if miner un-indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -i  | grep ' + file_path + ' | wc -l')
        print result
        self.assert_(result == '0' , 'deleted image file is shown as indexed')

    def test_music_02 (self):

        """ To check if tracker indexes music if its deleted in massstorage mode """

        file_path = MYDOCS + TEST_MUSIC

        """copy the test files """
        self.wait_for_fileop('cp', SRC_MUSIC_DIR + TEST_MUSIC, file_path)

        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path + ' | wc -l')
        print result
        self.assert_(result == '1' , 'copied file is not shown as indexed')

        check_mount()
        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount '  + MOUNT_PARTITION + ' ' + MOUNT_DUMMY)
	  
        """remove the test files """
        os.remove(MOUNT_DUMMY + '/' + TEST_MUSIC)


        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount '  + MOUNT_PARTITION + ' ' + MYDOCS)
	  

        print commands.getoutput ( 'ls ' + MYDOCS)

       
        self.loop.run ()

        """ verify if miner un-indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -m  | grep ' + file_path + ' | wc -l')
        print result
        self.assert_(result == '0' , 'deleted music file is shown as indexed')

    def test_video_03 (self):

        """ To check if tracker indexes video if its deleted in massstorage mode """

        file_path = MYDOCS + TEST_VIDEO

        """copy the test files """
        self.wait_for_fileop('cp', SRC_VIDEO_DIR + TEST_VIDEO, file_path)

        """ 2. verify if miner indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path + ' | wc -l')

	print result
        self.assert_(result == '1' , 'copied file is not shown as indexed')

        check_mount()
        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount '  + MOUNT_PARTITION + ' ' + MOUNT_DUMMY)
	  

        """remove the test files """
        os.remove(MOUNT_DUMMY + '/' + TEST_VIDEO)


        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount '  + MOUNT_PARTITION + ' ' + MYDOCS)
	  

        print commands.getoutput ( 'ls ' + MYDOCS)

      
        self.loop.run ()

        """ verify if miner un-indexed this file.  """
        result = commands.getoutput ('tracker-search --limit=10000  -v  | grep ' + file_path + ' | wc -l')
        print result
        self.assert_(result == '0' , 'deleted video file is shown as indexed')

class text (TestUpdate) :

    def test_text_01 (self):

    	""" To check if tracker indexes changes made to a text file  in massstorage mode """

	file_path =  MYDOCS + TEST_TEXT

	""" Creating text file """
	f1=open(file_path,"w")
	f1.writelines("This is a new text file")
	f1.close()


        check_mount()

        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount '  + MOUNT_PARTITION + ' ' + MOUNT_DUMMY)
	  
	self.edit_text(TEST_TEXT)

        commands.getoutput ( 'umount ' + MOUNT_PARTITION )
	  
        commands.getoutput ( 'mount -t vfat -o rw '  + MOUNT_PARTITION + ' ' + MYDOCS)
	  

        print commands.getoutput ( 'ls ' + MYDOCS)
	self.loop.run()

	""" verify if miner indexed these file.  """

        result = commands.getoutput ('tracker-search  -t  massstorage | grep ' + file_path + ' | wc -l')
        print result
        self.assert_(result == '1' , 'copied text file is not shown as indexed')

	os.remove(file_path)


class no_file_op(TestUpdate):

	def test_msm_01(self):

	     """1. check if tracker is idle. wait till it gets to idle state. """

	     check_mount()
             commands.getoutput ('umount /dev/mmcblk0p1')
	       
             result = self.miner.GetStatus ()
             self.assert_(result == 'Idle' , 'Tracker is not in idle state')

	def test_msm_02(self):

            """
	    1. unmount the MyDocs

            2. check if tracker-search -i is retrieving result """

	    check_mount()
	    commands.getoutput ('umount /dev/mmcblk0p1')
	      
	    result = commands.getoutput ('tracker-search -f -l 10000 | grep ' + MYDOCS + '  |wc -l ')
	    print result
	    self.assert_(result == '0' , 'Tracker is listing the files when the device is connected in mass storage mode')


	def test_msm_03(self):

            """1. unmount the MyDocs

               2. check if tracker-search -ia is retrieving result """

	    check_mount()
            commands.getoutput ('umount /dev/mmcblk0p1')
	      
            result = commands.getoutput ('tracker-search -f -l 10000 |wc -l ')
            self.assert_(result != 0 , 'Tracker(checked with -a) is not listing the files when the device is connected in mass storage mode')

if __name__ == "__main__":

        #unittest.main()
        copy_tcs_list=unittest.TestLoader().getTestCaseNames(copy)
        copy_testsuite=unittest.TestSuite(map(copy, copy_tcs_list))


        move_tcs_list=unittest.TestLoader().getTestCaseNames(move)
        move_testsuite=unittest.TestSuite(map(move, move_tcs_list))


        rename_tcs_list=unittest.TestLoader().getTestCaseNames(rename)
        rename_testsuite=unittest.TestSuite(map(rename, rename_tcs_list))

        delete_tcs_list=unittest.TestLoader().getTestCaseNames(delete)
        delete_testsuite=unittest.TestSuite(map(delete, delete_tcs_list))


        subfolder_tcs_list=unittest.TestLoader().getTestCaseNames(subfolder)
        subfolder_testsuite=unittest.TestSuite(map(subfolder, subfolder_tcs_list))


        text_tcs_list=unittest.TestLoader().getTestCaseNames(text)
        text_testsuite=unittest.TestSuite(map(text, text_tcs_list))

        file_tcs_list=unittest.TestLoader().getTestCaseNames(no_file_op)
        file_testsuite=unittest.TestSuite(map(no_file_op, file_tcs_list))

        all_testsuites = unittest.TestSuite((rename_testsuite,move_testsuite,copy_testsuite,subfolder_testsuite,text_testsuite))

        unittest.TextTestRunner(verbosity=2).run(all_testsuites)





