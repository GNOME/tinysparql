#!/usr/bin/python
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
import os 
import dbus # For the exception handling

from common.utils.system import TrackerSystemAbstraction
from common.utils.helpers import StoreHelper
from common.utils import configuration as cfg
from common.utils.storetest import CommonTrackerStoreTest as CommonTrackerStoreTest
from common.utils.expectedFailure import expectedFailureBug as expectedFailureBug
import unittest2 as ut


"""
Call backup, restore, force the journal replay and check the data is correct afterwards
"""
class BackupRestoreTest (CommonTrackerStoreTest):
        """
        Backup and restore to/from valid/invalid files
        """
        def setUp (self):
            self.TEST_INSTANCE = "test://backup-restore/1"
            self.BACKUP_FILE = "file://" + os.path.join (cfg.TEST_TMP_DIR, "tracker-backup-test-1")

            if (os.path.exists (self.BACKUP_FILE)):
                os.unlink (self.BACKUP_FILE)

        def __insert_test_instance (self):
            self.tracker.update ("INSERT { <%s> a nco:Contact; nco:fullname 'test-backup' } "
                                 % (self.TEST_INSTANCE))

        def __delete_test_instance (self):
            self.tracker.update ("DELETE { <%s> a rdfs:Resource } " % (self.TEST_INSTANCE))

        def __is_test_instance_there (self):
            result = self.tracker.query ("SELECT ?u WHERE { ?u a nco:Contact; nco:fullname 'test-backup'}")
            if (len (result) == 1 and len (result[0]) == 1 and result[0][0] == self.TEST_INSTANCE):
                return True
            return False

	def test_backup_01(self):
            """
            Inserted data is restored after backup
            
            1.Insert contact
            2.Take backup.
            3.Delete contact. (check it is not there)
            4.Restore the file.
            5.Check the contact is back there
            """
            
            self.__insert_test_instance ()
            instances_before = self.tracker.count_instances ("nco:Contact")
            
            self.tracker.backup (self.BACKUP_FILE)
            
            self.__delete_test_instance ()
            instances_now = self.tracker.count_instances ("nco:Contact")
            
            self.assertEquals (instances_before-1, instances_now)
            
            self.tracker.restore (self.BACKUP_FILE)

            instances_after = self.tracker.count_instances ("nco:Contact")

            self.assertEquals (instances_before, instances_after)
            self.assertTrue (self.__is_test_instance_there ())
            
            # Clean the DB for the next test
            self.__delete_test_instance ()


 	def test_backup_02 (self):
 	      """
              Data inserted after backup is lost in restore
              
 	      1.Take backup of db.
 	      2.Insert a contact.
 	      3.Restore the db.
 	      4.Search for the contact inserted. 
 	      """

              # Precondition: test backup contact shouldn't be there
              self.assertFalse (self.__is_test_instance_there ())

              self.tracker.backup (self.BACKUP_FILE)

              self.__insert_test_instance ()
              self.assertTrue (self.__is_test_instance_there ())

              self.tracker.restore (self.BACKUP_FILE)

              self.assertFalse (self.__is_test_instance_there ())


	
	def test_backup_03 (self):
	      """
              Restore from a random text file
	      """
              TEST_FILE = os.path.join (cfg.TEST_TMP_DIR, "trash_file")
              trashfile = open (TEST_FILE, "w")
              trashfile.write ("Here some useless text that obviously is NOT a backup")
              trashfile.close ()

	      self.assertRaises (dbus.DBusException,
                                 self.tracker.restore,
                                 "file://" + TEST_FILE)
              os.unlink (TEST_FILE)

        def test_backup_04 (self):
              """
              Restore from a random binary file
              """
              TEST_FILE = os.path.join (cfg.TEST_TMP_DIR, "trash_file.dat")
              
              import struct
              trashfile = open (TEST_FILE, "wb")
              for n in range (0, 50):
                  data = struct.pack ('i', n)
                  trashfile.write (data)
              trashfile.close ()

              instances_before = self.tracker.count_instances ("nie:InformationElement")
	      self.assertRaises (dbus.DBusException,
                                 self.tracker.restore,
                                 "file://" + TEST_FILE)

              os.unlink (TEST_FILE)

	def test_backup_05(self):
	      """
	      Take backup of db to a invalid path.
	      Expected: Backup should not be taken and tracker should behave normally.	
	      """
              self.assertRaises (dbus.DBusException,
                                 self.tracker.backup,
                                 "file://%s/this/is/a/non-existant/folder/backup" % (cfg.TEST_TMP_DIR))
              

        def test_backup_06 (self):
            """
            Try to restore an invalid path
            """
            self.assertRaises (dbus.DBusException,
                               self.tracker.restore,
                               "file://%s/this/is/a/non-existant/folder/backup" % (cfg.TEST_TMP_DIR))
		

	def test_backup_07(self):
	      """
              Restore after removing the DBs and journal
              
	      1.Insert a contact.
	      2.Take backup of db.
	      4.Delete the database
	      5.Restore the db.
	      6.Search for the contact inserted. 
              """
              self.__insert_test_instance ()
              instances_before = self.tracker.count_instances ("nco:Contact")
	      self.tracker.backup (self.BACKUP_FILE)

              self.system.tracker_store_remove_dbs ()
              self.system.tracker_store_remove_journal ()
              self.system.tracker_store_brutal_restart ()
              
              instances_before_restore = self.tracker.count_instances ("nco:Contact")
              self.assertNotEqual (instances_before_restore, instances_before)
              
              self.tracker.restore (self.BACKUP_FILE)
	      self.assertTrue (self.__is_test_instance_there ())

              self.__delete_test_instance ()


	def test_backup_08 (self):
	      """
              Restore after corrupting DB
              
	      1.Insert a contact.
	      2.Take backup of db.
	      5.Restore the db.
	      6.Search for the contact inserted. 
              """
              self.__insert_test_instance ()
              instances_before = self.tracker.count_instances ("nco:Contact")
	      self.tracker.backup (self.BACKUP_FILE)

              self.system.tracker_store_corrupt_dbs ()
              self.system.tracker_store_remove_journal ()
              self.system.tracker_store_brutal_restart ()
              
              instances_before_restore = self.tracker.count_instances ("nco:Contact")
              self.assertNotEqual (instances_before_restore, instances_before)
              
              self.tracker.restore (self.BACKUP_FILE)
	      self.assertTrue (self.__is_test_instance_there ())

              # DB to the original state
              self.__delete_test_instance ()
              

## 	def test_backup_09(self):
## 	      """
## 	      1.Insert a contact.
## 	      2.Take backup of db.
## 	      3.Corrupt the databse.
## 	      4.Restore the db.
## 	      5.Search for the contact inserted. 
## 	      Expected:Contact should be listed in tracker search.	
## 	      """

## 	      urn = 'urn:uuid:'+`random.randint(0, sys.maxint)`
## 	      insert_sparql = "INSERT { <%s> a nco:Contact } "
## 	      insert = insert_sparql %(urn)
## 	      self.sparql_update(insert)

## 	      result = commands.getoutput('tracker-stats | grep nco:Contact')
## 	      stats1 = result.split()

## 	      self.backup.Save(BACKUP_1)

##               delete_sparql = "DELETE { <%s> a nco:Contact } "                                                            
##               delete = delete_sparql %(urn)                                                                               
##               self.sparql_update(delete)  	
		
## 	      print ("Corrupting the database")

## 	      commands.getoutput('cp  ' + configuration.TEST_DATA_IMAGES +'test-image-1.jpg' + ' ' + database +'meta.db' )
## 	      try :
##                  self.db_restore(BACKUP_1)
## 	      except :
## 		 print "Restore is not scuccessful"

## 	      print "querying for the contact"
## 	      query_sparql = "SElECT ?c WHERE {?c a nco:Contact .FILTER (REGEX(?c, '%s' )) }"
##               query = query_sparql %(urn)                                             
##               result = self.query(query)                                                     
## 	      self.assert_(result[0][0].find(urn)!=-1 , "Restore is not successful" )

## 	def test_backup_10(self):

## 	      """
## 	      1.Insert a contact.
## 	      2.Take backup of db.
## 	      3.Kill tracker-store process.
## 	      4.Corrupt the databse.
## 	      5.Restore the db.
## 	      6.Search for the contact inserted. 
## 	      Expected:Contact should be listed in tracker search.	
## 	      """

## 	      urn = random.randint(0, sys.maxint)	

## 	      insert_sparql = "INSERT { <%s> a nco:Contact } "
## 	      insert = insert_sparql %(urn)

## 	      self.sparql_update(insert)

## 	      result = commands.getoutput('tracker-stats | grep nco:Contact')
## 	      stats1 = result.split()

## 	      self.backup.Save(BACKUP_1)

## 	      delete_sparql = "DELETE { <%s> a nco:Contact } "                                                            
##               delete = delete_sparql %(urn)                                                                               
##               self.sparql_update(delete)  

## 	      self.kill_store() 

## 	      print ("corrupting the database")
## 	      commands.getoutput('cp  ' + configuration.TEST_DATA_IMAGES +'test-image-1.jpg' + ' ' + database +'meta.db' )

## 	      try :
##                  self.db_restore(BACKUP_1)
## 	      except :
## 		 print "Restore is not scuccessful"

##               result = commands.getoutput('tracker-stats | grep nco:Contact ')
## 	      query_sparql = "SElECT ?c WHERE {?c a nco:Contact .FILTER (REGEX(?c, '%s' )) }"
##               query = query_sparql %(urn)                                                    
##               result = self.query(query)                                                     
## 	      self.assert_(result[0][0].find(urn)!=-1 , "Restore is not successful")

        def test_backup_11(self):
	      """
              Backup ignores the file extension
              
	      1.Insert a contact.
	      2.Take backup of db in .jpg format.
	      3.Restore the db.
	      4.Search for the contact inserted. 
	      """
              BACKUP_JPG_EXT = "file://%s/tracker-test-backup.jpg" % (cfg.TEST_TMP_DIR)
              
              self.__insert_test_instance ()

              instances_before = self.tracker.count_instances ("nco:Contact")

	      self.tracker.backup (BACKUP_JPG_EXT)

              self.__delete_test_instance ()
              instances_now = self.tracker.count_instances ("nco:Contact")
              self.assertEquals (instances_before, instances_now+1)

              self.tracker.restore (BACKUP_JPG_EXT)
              instances_after = self.tracker.count_instances ("nco:Contact")
              self.assertEquals (instances_before, instances_after)

              # Restore the DB to the original state
              self.__delete_test_instance ()



class JournalReplayTest (CommonTrackerStoreTest):
        """
        Force journal replaying and check that the DB is correct aftewards
        """
 	def test_journal_01 (self) :
            """
            Journal replaying when the DB is corrupted
            
            Insert few data (to have more than the pre-defined instances)
            Check instances of different classes
            Replace the DB with a random file
            Restart the daemon
            Check instances of different classes
            """
            self.tracker.update ("INSERT { <test://journal-replay/01> a nco:Contact. }")
            
            emails = self.tracker.count_instances ("nmo:Email")
            ie = self.tracker.count_instances ("nie:InformationElement")
            contacts = self.tracker.count_instances ("nco:Contact")

            self.system.tracker_store_corrupt_dbs ()
            self.system.tracker_store_brutal_restart ()
            ## Start it twice... the first time it detects the broken DB and aborts
            self.system.tracker_store_brutal_restart ()

            emails_now = self.tracker.count_instances ("nmo:Email")
            ie_now = self.tracker.count_instances ("nie:InformationElement")
            contacts_now = self.tracker.count_instances ("nco:Contact")

            self.assertEquals (emails, emails_now)
            self.assertEquals (ie, ie_now)
            self.assertEquals (contacts, contacts_now)

            self.tracker.update ("DELETE { <test://journal-replay/01> a rdfs:Resource. }")

	def test_journal_02 (self) :
            """
            Journal replaying when the DB disappears
            
            Insert few data (to have more than the pre-defined instances)
            Check instances of different classes
            Remove the DB
            Restart the daemon
            Check instances of different classes
            """
            self.tracker.update ("INSERT { <test://journal-replay/02> a nco:Contact. }")
            
            emails = self.tracker.count_instances ("nmo:Email")
            ie = self.tracker.count_instances ("nie:InformationElement")
            contacts = self.tracker.count_instances ("nco:Contact")


            self.system.tracker_store_prepare_journal_replay ()
        
            self.system.tracker_store_brutal_restart ()

            emails_now = self.tracker.count_instances ("nmo:Email")
            ie_now = self.tracker.count_instances ("nie:InformationElement")
            contacts_now = self.tracker.count_instances ("nco:Contact")

            self.assertEquals (emails, emails_now)
            self.assertEquals (ie, ie_now)
            self.assertEquals (contacts, contacts_now)

            self.tracker.update ("DELETE { <test://journal-replay/02> a rdfs:Resource. }")

if __name__ == "__main__":
    ut.main()                      



