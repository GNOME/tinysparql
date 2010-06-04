import dbus
import os
from dbus.mainloop.glib import DBusGMainLoop
import gobject
import unittest
import time
import commands, signal

TRACKER = 'org.freedesktop.Tracker1'
TRACKER_OBJ = '/org/freedesktop/Tracker1/Resources'

TEST_INSTANCE_PATTERN = "test://test-instance-%d"

def kill_store ():
    for pid in commands.getoutput("ps -ef| grep tracker-store | awk '{print $2}'").split() :
        try:
            print "Killing tracker process", pid
            os.kill(int(pid), signal.SIGKILL)
        except OSError, e:
            if not e.errno == 3 : raise e

class TrackerTransactionsTest (unittest.TestCase):

    def setUp (self):
        self.connect_dbus ()
        self.instance_counter = 0

    def connect_dbus (self):
        dbus_loop = DBusGMainLoop(set_as_default=True)
        self.bus = dbus.SessionBus (dbus_loop)
        self.tracker = self.bus.get_object (TRACKER, TRACKER_OBJ)
        self.iface = dbus.Interface (self.tracker,
                                     "org.freedesktop.Tracker1.Resources")


    def tearDown (self):
        print "Tear down (will take some time to remove all resources)"
        delete_sparql = "DELETE { ?u a rdfs:Resource } WHERE { ?u a nmo:Email} \n"
        self.iface.SparqlUpdate (delete_sparql,
                                 #reply_handler=self.delete_ok_cb,
                                 #error_handler=self.delete_error_cb,
                                 timeout=60000)
        self.instance_counter = 0

    def delete_ok_cb (self):
        print "Delete ok"

    def delete_error_cb (self, error):
        print "Delete error"
    
    def insert_and_commit (self, number):
        print "Preparing the batch sparql"
        insert_sparql = "INSERT {\n"
        for i in range (0, number):
            insert_sparql += "  <" + TEST_INSTANCE_PATTERN % (self.instance_counter) + ">"
            insert_sparql += " a nmo:Email.\n "
            self.instance_counter += 1

        insert_sparql += "}"
        self.iface.BatchSparqlUpdate (insert_sparql)
        print "Waiting for commit (", number," instances)"
        start = time.time ()
        self.iface.BatchCommit ()
        end = time.time ()
        print "BatchCommit returned (after %d s.)" % (end - start)

    def count_instances (self):
        #query_sparql = "SELECT COUNT(?u) WHERE { ?u a nmo:Email. FILTER (REGEX (?u, '%s*'))}" % TEST_INSTANCE_PATTERN[:-3]
        query_sparql = "SELECT COUNT(?u) WHERE { ?u a nmo:Email.}"
        return int (self.iface.SparqlQuery (query_sparql)[0][0])

    def test_commit_and_abort (self):

        for i in range (0, 20):
            NUMBER_OF_INSTANCES = 100
            self.insert_and_commit (NUMBER_OF_INSTANCES)

            print "Abort daemon"
            kill_store ()
            time.sleep (3)
            # Reconnect dbus to autoactivate the daemon!
            self.connect_dbus ()
            try:
                print "Wake up the store with a query"
                results = self.count_instances ()
            except:
                print "Timeout, probably replaying journal or something (wait 20 sec.)"
                time.sleep (20)
                results = self.count_instances ()

            self.assertEqual (results, NUMBER_OF_INSTANCES * (i+1))
            print "Yep,", results, "results"

if __name__ == "__main__":
    unittest.main ()
