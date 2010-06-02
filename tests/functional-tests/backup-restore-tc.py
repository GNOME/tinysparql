#!/usr/bin/env python
#
# Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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



import sys,os,dbus
import unittest
import time
import random
import commands
import signal
import string
import configuration
from dbus.mainloop.glib import DBusGMainLoop
import gobject


TRACKER = 'org.freedesktop.Tracker1'                                                     
TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources'                                      
RESOURCES_IFACE = "org.freedesktop.Tracker1.Resources"                                   

BACKUP1 = 'org.freedesktop.Tracker1'                                            
BACKUP = "/org/freedesktop/Tracker1/Backup"                                            
BACKUP_IFACE = "org.freedesktop.Tracker1.Backup"

STATUS = '/org/freedesktop/Tracker1/Status'
STATUS_IFACE = 'org.freedesktop.Tracker1.Status'

target = configuration.check_target()

if target == configuration.MAEMO6_HW:
    """target is device """
    BACKUP_1 = configuration.URL_PREFIX+configuration.MYDOCS + "test_backup_1"
    BACKUP_2 = configuration.URL_PREFIX+configuration.MYDOCS + "test_backup_2"
    BACKUP_INVALID = configuration.URL_PREFIX+configuration.URL_PREFIX + '/a/b/c'
    BACKUP_JPG = configuration.URL_PREFIX+configuration.MYDOCS + "test_backup.jpg"
elif target == configuration.DESKTOP:                                      
    """target is DESKTOP """                                               
    BACKUP_1 = os.path.expanduser("~") + '/' + "test_backup_1"       
    BACKUP_2 = os.path.expanduser("~") + '/' + "test_backup_2"  
    BACKUP_INVALID = configuration.URL_PREFIX + '/a/b/c'
    BACKUP_JPG = os.path.expanduser("~") + '/' + "test_backup.jpg"  



database = "/home/user/.cache/tracker/"

class TestHelper (unittest.TestCase):
	def setUp(self):                                                                 
                bus = dbus.SessionBus()                                                  
                tracker = bus.get_object(TRACKER, TRACKER_OBJ)                           
                self.resources = dbus.Interface (tracker, dbus_interface=RESOURCES_IFACE)
                backup_obj = bus.get_object(TRACKER,BACKUP)                                  
                self.backup = dbus.Interface (backup_obj,dbus_interface = BACKUP_IFACE)  
		status_obj  = bus.get_object(TRACKER,STATUS)
                self.status = dbus.Interface ( status_obj , dbus_interface = STATUS_IFACE)

                self.loop = gobject.MainLoop()
                self.dbus_loop = DBusGMainLoop(set_as_default=True)
                self.bus = dbus.SessionBus (self.dbus_loop)

                self.bus.add_signal_receiver (self.restore,
                                             signal_name="Progress",
                                             dbus_interface=STATUS_IFACE,
                                             path=STATUS)

	def db_backup(self,backup_file) :
		self.backup.Save(backup_file,timeout=5000)
	
	def db_restore(self,backup_file):
		return  self.backup.Restore(backup_file,timeout=5000)

        def sparql_update(self,query):
                self.resources.SparqlUpdate(query,timeout=5000)

        def query(self,query):
                return self.resources.SparqlQuery(query,timeout=5000)

        def restore(self,signal,handle) :
                if signal == "Journal replaying" :
                    print "Got journal replaying signal"
		    set()
                    if handle == 1:
		       print "Journal replaying is done"
                       self.loop.quit()


	def dict(self,list_name):
	      dd={}
              for a1 in list_name :
                key,value = a1.split('=')
                if key in dd :
                   vlist = dd [key]
                   if value not in list_name  :
                        vlist.append(value)
                else :
                    dd[key]=[value]
	      return dd

        def kill_store(self):
            for pid in commands.getoutput("ps -ef| grep /usr/lib/tracker/tracker-store | awk '{print $1}'").split() :
                try:
                   print "Killing tracker process"
                   print pid
                   os.kill(int(pid), signal.SIGKILL)
                except OSError, e:
                   if not e.errno == 3 : raise e
	def start_store (self) :
            os.system('/usr/lib/tracker/tracker-store  -v 3 &' )


class backup(TestHelper):

	""" Check statistics before and after back/restore. """

	def test_backup_01(self):
	      """
		 1.Insert contact
		 2.Check tracker-stats.
		 3.Take backup.
                 4.Delete contact.
		 5.Restore the file.
		 6.Check tracker-stats.
		 7.Compare the statistics. 

	         Expected:tracker-stats should not be changed after restore.

	      """
		
	      insert_sparql = "INSERT { <%s> a nco:Contact } "                           
	      INSERT = insert_sparql %(urn)                                              
	      self.resources.SparqlUpdate(INSERT) 

	      result = commands.getoutput('tracker-stats ')
	      stats1 = result.split('\n')
	      stats1.remove('Statistics:')
	      dd1 = self.dict(stats1)	

	      self.db_backup(BACKUP_1)

	      delete_sparql = "DELETE { <%s> a nco:Contact } "      
	      delete = delete_sparql %(urn)                         
              self.sparql_update(delete)

	      try :
                 self.db_restore(BACKUP_1)
	      except :
		  print "Restore is not successful"

              result = commands.getoutput('tracker-stats')
	      stats2 = result.split('\n')
	      stats2.remove('Statistics:')
	      dd2 = self.dict(stats2)

	      diff = [key for key, value in dd1.iteritems() if value != dd2[key]]

	      self.assert_(len(diff) == 0 , "Folling Statistics are changed %s" %diff)

	""" Check if the database is restored successfully """	      
	def test_backup_02(self):
	      """
	      1.Take backup of db.
	      2.Insert a contact.
	      3.Restore the db.
	      4.Search for the contact inserted. 
	      Expected:Contact should not be listed in tracker search.	
	      self.backup.Save(BACKUP_1)

	      """

	      self.db_backup(BACKUP_1)

              urn = 'urn:uuid:' + `random.randint(0, sys.maxint)`
	      insert_sparql = "INSERT { <%s> a nco:Contact } "
	      insert = insert_sparql %(urn)
	      self.sparql_update(insert)

	      result = commands.getoutput('tracker-stats | grep nco:Contact')
	      stats1 = result.split()
	      try :
                 self.db_restore(BACKUP_1)
	      except :
 		 print "Restore is not scuccessful"
              result = commands.getoutput('tracker-stats | grep nco:Contact ')
              stats2 = result.split()          
	      self.assert_(int (stats2[2]) ==int (stats1[2] )-1 , "Database is not restored successfully ")

	""" Checking backup and restore functionality """ 

	def test_backup_03(self):

	      """
	       	1.Insert a contact.
		2.Take backup of db.
	        3.Delete the contact.
		4.Restore the db.
		5.Search for contact inserted in first step.
		Expected:Tracker search should list the contact name
	      """

              urn = 'urn:uuid:'+`random.randint(0, sys.maxint)`
	      insert_sparql = "INSERT { <%s> a nco:Contact } "
	      INSERT = insert_sparql %(urn)  
	      self.resources.SparqlUpdate(INSERT)

	      self.db_backup(BACKUP_1)

	      delete_sparql = "DELETE { <%s> a nco:Contact } "
              delete = delete_sparql %(urn)
	      self.sparql_update(delete)
	        	      
	      try :
                 self.db_restore(BACKUP_1)
	      except :
		 print "Restore is not scuccessful"

	      query_sparql = "SElECT ?c WHERE {?c a nco:Contact .FILTER (REGEX(?c, '%s' )) }"
	      query = query_sparql %(urn)
	      result = self.query(query)

	      self.assert_(result[0][0].find(urn)!= -1 , "Restore is not successful" )


	"""Corrupt the backup file """
	
	def test_backup_04(self):

	      """
	      1.Take backup of db.
	      2.Corrupt the backup file
	      3.Restore the db.
	      Expected:Tracker should behave normally.	
	      """
	      	     
	      self.backup.Save(BACKUP_1)

	      commands.getoutput('cp /home/user/MyDocs/testing/Images/metadata.jpg file')

	      try :
                 self.db_restore(BACKUP_1)
	      except :
		 self.assert_(True,"Restore is not scuccessful")


	"""Take backup in a file with invalid path """
	
	def test_backup_05(self):
	      """
	      1.Take backup of db to a invalid path.
	      2.Restore the db.

	      Expected:Backup should not be taken and tracker should behave normally.	
	      """
	      	     
	      try :
		self.db_backup(BACKUP_INVALID)
	      except :
		print "Not able to take backup"
                self.assert_(True,'Backup is successful')


	"""Restore a file with invalid path """
	
	def test_backup_06(self):
	      """
	      1.Take backup of db.
	      2.Restore the db from an invalid path.

	      Expected: Restore should not happend .Tracker should behave normally.	
	      """
	      self.db_backup(BACKUP_1)
	      try :
		self.db_restore(BACKUP_INVALID)
	      except :
		print " Restore is not succssful "
		self.assert_(True,'Restore is successful')
		
	""" Check if the database is restored successfully """	      
	def test_backup_07(self):
	      """
	      1.Insert a contact.
	      2.Take backup of db.
	      3.Delete contact.
	      4.Delete the database
	      5.Restore the db.
	      6.Search for the contact inserted. 

	      Expected:Contact should be listed in tracker search.	
	      """
	      urn = 'urn:uuid:'+`random.randint(0, sys.maxint)`
	      insert_sparql = "INSERT { <%s> a nco:Contact } "
	      insert = insert_sparql %(urn)
	      self.sparql_update(insert)

	      result = commands.getoutput('tracker-stats | grep nco:Contact')
	      stats1 = result.split()

	      self.db_backup(BACKUP_1)

	      delete_sparql = "DELETE { <%s> a nco:Contact } "               
              delete = delete_sparql %(urn)                                  
              self.sparql_update(delete)

	      print ("Deleting the database")
	      commands.getoutput('rm -rf ' + database)

	      try :
                 self.db_restore(BACKUP_1)
	      except :
		 print "Restore is not scuccessful"


	      query_sparql = "SElECT ?c WHERE {?c a nco:Contact .FILTER (REGEX(?c, '%s' )) }"
              query = query_sparql %(urn)                                                                                 
              result = self.query(query)                                                     
              self.assert_(result[0][0].find(urn)!= -1 , "Restore is not successful" ) 
	      

	""" Check if the database is restored successfully """	      
	def test_backup_08(self):
	      """
	      1.Insert a contact.
	      2.Take backup of db.
	      3.Delete the contact.
	      4.Kill tracker-store process.
	      5.Delete the databse.
	      6.Restore the db.
	      7.Search for the contact inserted. 

	      Expected:Contact should be listed in tracker search.	

	      """

	      urn = 'urn:uuid:'+ `random.randint(0, sys.maxint)	`
	      insert_sparql = "INSERT { <%s> a nco:Contact } "
	      insert = insert_sparql %(urn)
	      self.sparql_update(insert)

	      result = commands.getoutput('tracker-stats | grep nco:Contact')
	      stats1 = result.split()

	      self.backup.Save(BACKUP_1)

	      delete_sparql = "DELETE { <%s> a nco:Contact } "                               
              delete = delete_sparql %(urn)                                                  
              self.sparql_update(delete)                                                     
                                           
	      print "Killing tracker store"
	      self.kill_store() 

	      print "Deleting the database"
	      commands.getoutput('rm -rf ' + database )

	      try :
                 self.db_restore(BACKUP_1)
	      except :
		 print "Restore is not scuccessful"

              result = commands.getoutput('tracker-stats | grep nco:Contact ')
	      query_sparql = "SElECT ?c WHERE {?c a nco:Contact .FILTER (REGEX(?c, '%s' )) }"
              query = query_sparql %(urn)                                                    
              result = self.query(query)                                                     
              self.assert_(result[0][0].find(urn)!=-1 , "Restore is not successful" ) 

	def test_backup_09(self):
	      """
	      1.Insert a contact.
	      2.Take backup of db.
	      3.Corrupt the databse.
	      4.Restore the db.
	      5.Search for the contact inserted. 
	      Expected:Contact should be listed in tracker search.	
	      """

	      urn = 'urn:uuid:'+`random.randint(0, sys.maxint)`
	      insert_sparql = "INSERT { <%s> a nco:Contact } "
	      insert = insert_sparql %(urn)
	      self.sparql_update(insert)

	      result = commands.getoutput('tracker-stats | grep nco:Contact')
	      stats1 = result.split()

	      self.backup.Save(BACKUP_1)

              delete_sparql = "DELETE { <%s> a nco:Contact } "                                                            
              delete = delete_sparql %(urn)                                                                               
              self.sparql_update(delete)  	
		
	      print ("Corrupting the database")

	      commands.getoutput('cp  ' + configuration.TEST_DATA_IMAGES +'test-image-1.jpg' + ' ' + database +'meta.db' )
	      try :
                 self.db_restore(BACKUP_1)
	      except :
		 print "Restore is not scuccessful"

	      print "querying for the contact"
	      query_sparql = "SElECT ?c WHERE {?c a nco:Contact .FILTER (REGEX(?c, '%s' )) }"
              query = query_sparql %(urn)                                             
              result = self.query(query)                                                     
	      self.assert_(result[0][0].find(urn)!=-1 , "Restore is not successful" )

	def test_backup_10(self):

	      """
	      1.Insert a contact.
	      2.Take backup of db.
	      3.Kill tracker-store process.
	      4.Corrupt the databse.
	      5.Restore the db.
	      6.Search for the contact inserted. 
	      Expected:Contact should be listed in tracker search.	
	      """

	      urn = random.randint(0, sys.maxint)	

	      insert_sparql = "INSERT { <%s> a nco:Contact } "
	      insert = insert_sparql %(urn)

	      self.sparql_update(insert)

	      result = commands.getoutput('tracker-stats | grep nco:Contact')
	      stats1 = result.split()

	      self.backup.Save(BACKUP_1)

	      delete_sparql = "DELETE { <%s> a nco:Contact } "                                                            
              delete = delete_sparql %(urn)                                                                               
              self.sparql_update(delete)  

	      self.kill_store() 

	      print ("corrupting the database")
	      commands.getoutput('cp  ' + configuration.TEST_DATA_IMAGES +'test-image-1.jpg' + ' ' + database +'meta.db' )

	      try :
                 self.db_restore(BACKUP_1)
	      except :
		 print "Restore is not scuccessful"

              result = commands.getoutput('tracker-stats | grep nco:Contact ')
	      query_sparql = "SElECT ?c WHERE {?c a nco:Contact .FILTER (REGEX(?c, '%s' )) }"
              query = query_sparql %(urn)                                                    
              result = self.query(query)                                                     
	      self.assert_(result[0][0].find(urn)!=-1 , "Restore is not successful")

	def test_backup_11(self):
	      """
	      1.Insert a contact.
	      2.Take backup of db in .jpg format.
	      3.Restore the db.
	      4.Search for the contact inserted. 
	      Expected:Contact should be listed in tracker search.	
	      """

	      urn = random.randint(0, sys.maxint)	
	      insert_sparql = "INSERT { <%s> a nco:Contact } "
	      insert = insert_sparql %(urn)

	      self.sparql_update(insert)

	      result = commands.getoutput('tracker-stats | grep nco:Contact')
	      stats1 = result.split()

	      self.backup.Save(BACKUP_JPG)

	      delete_sparql = "DELETE { <%s> a nco:Contact } "                                                            
              delete = delete_sparql %(urn)                                                                               
              self.sparql_update(delete)  
	      try :
                 self.db_restore(BACKUP_1)
	      except :
		 print "Restore is not scuccessful"

              result = commands.getoutput('tracker-stats | grep nco:Contact ')
	      query_sparql = "SElECT ?c WHERE {?c a nco:Contact .FILTER (REGEX(?c, '%s' )) }"
              query = query_sparql %(urn)                                                    
              result = self.query(query)                                                     
	      self.assert_(result[0][0].find(urn)!=-1 , "Restore is not successful") 



class journal (TestHelper) :

	def test_journal_01 (self) :

	    result1 = commands.getoutput('tracker-stats ')
            stats1 = result1.split('\n')
            stats1.remove('Statistics:')
            dd1 = self.dict(stats1)

	    print "killing tracker-store process"
	    self.kill_store ()

	    print ("Corrupting the database")
            commands.getoutput('cp  ' + configuration.TEST_DATA_IMAGES +'test-image-1.jpg' + ' ' + database +'meta.db' )

	    print "Starting tracker-store process"
	    self.start_store ()
	    self.loop.run()

            result2 = commands.getoutput('tracker-stats ')
            stats2 = result2.split('\n')
            stats2.remove('Statistics:')
            dd2 = self.dict(stats1)
	    diff = [key for key, value in dd1.iteritems() if value != dd2[key]]

            self.assert_(isSet() == true and len(diff)==0, 'Journal replaying is not happening' )


	def test_journal_02 (self) :

	    result1 = commands.getoutput('tracker-stats ')
            stats1 = result1.split('\n')
            stats1.remove('Statistics:')
            dd1 = self.dict(stats1)

	    print "killing tracker-store process"
	    self.kill_store ()

	    print ("Deleting the database")
            commands.getoutput('rm -rf ' + database )

	    print "Starting tracker-store process"
	    self.start_store ()

	    self.loop.run()

            result2 = commands.getoutput('tracker-stats ')
            stats2 = result2.split('\n')
            stats2.remove('Statistics:')
            dd2 = self.dict(stats1)
	    diff = [key for key, value in dd1.iteritems() if value != dd2[key]]

            self.assert_(isSet() == true and len(diff)==0, 'Journal replaying is not happening' )

if __name__ == "__main__":
        unittest.main()                      



