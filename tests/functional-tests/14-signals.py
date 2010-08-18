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
"""
Test that after insertion/remove/updates in the store, the signals
are emitted. Theses tests are not extensive (only few selected signals
are tested)
"""

import unittest2 as ut
from common.utils.storetest import CommonTrackerStoreTest as CommonTrackerStoreTest
from common.utils import configuration as cfg

import gobject
import glib
import dbus
from dbus.mainloop.glib import DBusGMainLoop

SUBJECTS_ADDED_SIGNAL = "SubjectsAdded"
SUBJECTS_REMOVED_SIGNAL = "SubjectsRemoved"
SUBJECTS_CHANGED_SIGNAL = "SubjectsChanged"

NCO_CONTACT_PATH = "/org/freedesktop/Tracker1/Resources/Classes/nco/Contact"
SIGNALS_IFACE = "org.freedesktop.Tracker1.Resources.Class"

REASONABLE_TIMEOUT = 10 # Time waiting for the signal to be emitted

class TrackerStoreSignalsTests (CommonTrackerStoreTest):
    """
    Insert/update/remove instances from nco:PersonContact
    and check that the signals are emitted
    """
    def setUp (self):
        self.loop = gobject.MainLoop()
        dbus_loop = DBusGMainLoop(set_as_default=True)
        self.bus = dbus.SessionBus (dbus_loop)
        self.timeout_id = 0

    def __connect_signal (self, signal_name, callback):
        """
        After connecting to the signal, call self.__wait_for_signal.
        That function will wait in a loop, so make sure that the callback
        calls self.loop.quit ()
        """
        if not signal_name in [SUBJECTS_ADDED_SIGNAL, SUBJECTS_REMOVED_SIGNAL, SUBJECTS_CHANGED_SIGNAL]:
            print "What kind of signal are you trying to connect?!"
            assert False

        self.cb_id = self.bus.add_signal_receiver (callback,
                                                   signal_name=signal_name,
                                                   path = NCO_CONTACT_PATH,
                                                   dbus_interface = SIGNALS_IFACE)

    def __wait_for_signal (self):
        """
        In the callback of the signals, there should be a self.loop.quit ()
        """
        self.timeout_id = glib.timeout_add_seconds (REASONABLE_TIMEOUT, self.__timeout_on_idle)
        self.loop.run ()

    def __timeout_on_idle (self):
        self.loop.quit ()
        self.fail ("Timeout, the signal never came!")


    def __disconnect_signals_after_test (fn):
        """
        Here maybe i got a bit carried away with python instrospection.
        This decorator makes the function run in a try/finally, and disconnect
        all the signals afterwards.

        It means that the signal callbacks just need to ensure the results are fine.
        Don't touch this unless you know what are you doing.
        """
        def new (self, *args):
            try:
                fn (self, *args)
            finally:
                if (self.timeout_id != 0):
                    glib.source_remove (self.timeout_id )
                    self.timeout_id = 0
                self.loop.quit ()
                self.bus._clean_up_signal_match (self.cb_id)
        return new
        
        
    @__disconnect_signals_after_test 
    def __contact_added_cb (self, contacts_added):
        self.assertEquals (len (contacts_added), 1)
        self.assertIn ("test://signals-contact-add", contacts_added)

    def test_01_insert_contact (self):
        CONTACT = """
        INSERT {
         <test://signals-contact-add> a nco:PersonContact ;
             nco:nameGiven 'Contact-name added';
             nco:nameFamily 'Contact-family added';
             nie:generator 'test-14-signals' ;
             nco:contactUID '1321321312312';
             nco:hasPhoneNumber <tel:555555555> .
        }
        """
        self.__connect_signal (SUBJECTS_ADDED_SIGNAL, self.__contact_added_cb)
        self.tracker.update (CONTACT)
        self.__wait_for_signal ()

        self.tracker.update ("""
        DELETE { <test://signals-contact-add> a rdfs:Resource }
        """)


    @__disconnect_signals_after_test
    def __contact_removed_cb (self, contacts_removed):
        self.assertEquals (len (contacts_removed), 1)
        self.assertIn ("test://signals-contact-remove", contacts_removed)

    def test_02_remove_contact (self):
        CONTACT = """
        INSERT {
         <test://signals-contact-remove> a nco:PersonContact ;
             nco:nameGiven 'Contact-name removed';
             nco:nameFamily 'Contact-family removed'.
        }
        """
        self.tracker.update (CONTACT)

        self.__connect_signal (SUBJECTS_REMOVED_SIGNAL, self.__contact_removed_cb)
        self.tracker.update ("""
            DELETE { <test://signals-contact-remove> a rdfs:Resource }
            """)
        self.__wait_for_signal ()
        


    @__disconnect_signals_after_test
    def __contact_updated_cb (self, contacts_updated, props_updated):
        self.assertEquals (len (contacts_updated), 1)
        self.assertIn ("test://signals-contact-update", contacts_updated)

        self.assertEquals (len (props_updated), 1)
        self.assertIn ("http://www.semanticdesktop.org/ontologies/2007/03/22/nco#fullname", props_updated)

    def test_03_update_contact (self):
        self.tracker.update ("INSERT { <test://signals-contact-update> a nco:PersonContact }")
        
        self.__connect_signal (SUBJECTS_CHANGED_SIGNAL, self.__contact_updated_cb)
        self.tracker.update ("INSERT { <test://signals-contact-update> nco:fullname 'wohoo'}")
        self.__wait_for_signal ()

        self.tracker.update ("DELETE { <test://signals-contact-update> a rdfs:Resource}")


if __name__ == "__main__":
    ut.main()

