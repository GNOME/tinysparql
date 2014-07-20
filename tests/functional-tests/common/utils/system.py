#!/usr/bin/python
import os
import subprocess
import shutil
import configuration as cfg

from gi.repository import GObject
from gi.repository import GLib
import dbus
from dbus.mainloop.glib import DBusGMainLoop
import time

import options
from dconf import DConfClient

import helpers

# Add this after fixing the backup/restore and ontology changes tests
#"G_DEBUG" : "fatal_criticals",

TEST_ENV_DIRS =  { "XDG_DATA_HOME" : os.path.join (cfg.TEST_TMP_DIR, "data"),
                   "XDG_CACHE_HOME": os.path.join (cfg.TEST_TMP_DIR, "cache")}

TEST_ENV_VARS = {  "TRACKER_DISABLE_MEEGOTOUCH_LOCALE": "",
                   "LC_COLLATE": "en_GB.utf8",
                   "DCONF_PROFILE": os.path.join (cfg.DATADIR, "tracker-tests",
                                                  "trackertest") }

EXTRA_DIRS = [os.path.join (cfg.TEST_TMP_DIR, "data", "tracker"),
              os.path.join (cfg.TEST_TMP_DIR, "cache", "tracker")]

REASONABLE_TIMEOUT = 30

class UnableToBootException (Exception):
    pass


class TrackerSystemAbstraction:

    def set_up_environment (self, settings, ontodir):
        """
        Sets up the XDG_*_HOME variables and make sure the directories exist

        Settings should be a dict mapping schema names to dicts that hold the
        settings that should be changed in those schemas. The contents dicts
        should map key->value, where key is a key name and value is a suitable
        GLib.Variant instance.
        """

        helpers.log ("[Conf] Setting test environment...")

        for var, directory in TEST_ENV_DIRS.iteritems ():
            helpers.log ("export %s=%s" %(var, directory))
            self.__recreate_directory (directory)
            os.environ [var] = directory

        for directory in EXTRA_DIRS:
            self.__recreate_directory (directory)

        if ontodir:
            helpers.log ("export %s=%s" % ("TRACKER_DB_ONTOLOGIES_DIR", ontodir))
            os.environ ["TRACKER_DB_ONTOLOGIES_DIR"] = ontodir

        for var, value in TEST_ENV_VARS.iteritems ():
            helpers.log ("export %s=%s" %(var, value))
            os.environ [var] = value

        # Previous loop should have set DCONF_PROFILE to the test location
        if settings is not None:
            self._apply_settings(settings)

        helpers.log ("[Conf] environment ready")

    def _apply_settings(self, settings):
        for schema_name, contents in settings.iteritems():
            dconf = DConfClient(schema_name)
            dconf.reset()
            for key, value in contents.iteritems():
                dconf.write(key, value)

    def unset_up_environment (self):
        """
        Unset the XDG_*_HOME variables from the environment
        """
        for var, directory in TEST_ENV_VARS.iteritems ():
            if os.environ.has_key (var):
                del os.environ [var]

        for var, directory in TEST_ENV_DIRS.iteritems ():
            if os.environ.has_key (var):
                del os.environ [var]

        if (os.environ.has_key ("TRACKER_DB_ONTOLOGIES_DIR")):
            del os.environ ["TRACKER_DB_ONTOLOGIES_DIR"]


    def tracker_store_testing_start (self, confdir=None, ontodir=None):
        """
        Stops any previous instance of the store, calls set_up_environment,
        and starts a new instances of the store
        """
        self.__stop_tracker_processes ()
        self.set_up_environment (confdir, ontodir)

        self.store = helpers.StoreHelper ()
        self.store.start ()

    def tracker_store_start (self):
        self.store.start ()

    def tracker_store_stop_nicely (self):
        self.store.stop ()

    def tracker_store_stop_brutally (self):
        self.store.kill ()

    def tracker_store_restart_with_new_ontologies (self, ontodir):
        self.store.stop ()
        if ontodir:
            helpers.log ("[Conf] Setting %s - %s" % ("TRACKER_DB_ONTOLOGIES_DIR", ontodir))
            os.environ ["TRACKER_DB_ONTOLOGIES_DIR"] = ontodir
        try:
            self.store.start ()
        except dbus.DBusException, e:
            raise UnableToBootException ("Unable to boot the store \n(" + str(e) + ")")

    def tracker_store_prepare_journal_replay (self):
        db_location = os.path.join (TEST_ENV_DIRS ['XDG_CACHE_HOME'], "tracker", "meta.db")
        os.unlink (db_location)

        lockfile = os.path.join (TEST_ENV_DIRS ['XDG_DATA_HOME'], "tracker", "data", ".ismeta.running")
        f = open (lockfile, 'w')
        f.write (" ")
        f.close ()

    def tracker_store_corrupt_dbs (self):
        for filename in ["meta.db", "meta.db-wal"]:
            db_path = os.path.join (TEST_ENV_DIRS ['XDG_CACHE_HOME'], "tracker", filename)
            f = open (db_path, "w")
            for i in range (0, 100):
                f.write ("Some stupid content... hohohoho, not a sqlite file anymore!\n")
            f.close ()

    def tracker_store_remove_journal (self):
        db_location = os.path.join (TEST_ENV_DIRS ['XDG_DATA_HOME'], "tracker", "data")
        shutil.rmtree (db_location)
        os.mkdir (db_location)

    def tracker_store_remove_dbs (self):
        db_location = os.path.join (TEST_ENV_DIRS ['XDG_CACHE_HOME'], "tracker")
        shutil.rmtree (db_location)
        os.mkdir (db_location)

    def tracker_store_testing_stop (self):
        """
        Stops a running tracker-store and unset all the XDG_*_HOME vars
        """
        assert self.store
        self.store.stop ()
        self.unset_up_environment ()


    def tracker_miner_fs_testing_start (self, confdir=None):
        """
        Stops any previous instance of the store and miner, calls set_up_environment,
        and starts a new instance of the store and miner-fs
        """
        self.__stop_tracker_processes ()
        self.set_up_environment (confdir, None)

        # Start also the store. DBus autoactivation ignores the env variables.
        self.store = helpers.StoreHelper ()
        self.store.start ()

        self.extractor = helpers.ExtractorHelper ()
        self.extractor.start ()

        self.miner_fs = helpers.MinerFsHelper ()
        self.miner_fs.start ()

    def tracker_miner_fs_wait_for_idle (self, timeout=REASONABLE_TIMEOUT):
        """
        Copy the files physically in the filesyste and wait for the miner to complete the work
        """
        self.miner_fs.wait_for_idle (timeout)


    def tracker_miner_fs_testing_stop (self):
        """
        Stops the miner-fs and store running and unset all the XDG_*_HOME vars
        """
        self.miner_fs.stop ()
        self.store.stop ()

        self.__stop_tracker_processes ()
        self.unset_up_environment ()


    def tracker_writeback_testing_start (self, confdir=None):
        # Start the miner-fs (and store) and then the writeback process
        self.tracker_miner_fs_testing_start (confdir)
        self.writeback = helpers.WritebackHelper ()
        self.writeback.start ()

    def tracker_writeback_testing_stop (self):
        # Tracker write must have been started before
        self.writeback.stop ()
        self.tracker_miner_fs_testing_stop ()

    def tracker_all_testing_start (self, confdir=None):
        # This will start all miner-fs, store and writeback
        self.tracker_writeback_testing_start (confdir)

    def tracker_all_testing_stop (self):
        # This will stop all miner-fs, store and writeback
        self.tracker_writeback_testing_stop ()

    #
    # Private API
    #
    def __stop_tracker_processes (self):
        control_binary = os.path.join (cfg.BINDIR, "tracker-control")
        FNULL = open('/dev/null', 'w')
        subprocess.call ([control_binary, "-t"], stdout=FNULL)
        time.sleep (1)

    def __recreate_directory (self, directory):
        if (os.path.exists (directory)):
            shutil.rmtree (directory)
        os.makedirs (directory)


if __name__ == "__main__":
    import gtk, glib, time

    def destroy_the_world (a):
        a.tracker_store_testing_stop ()
        print "   stopped"
        Gtk.main_quit()

    print "-- Starting store --"
    a = TrackerSystemAbstraction ()
    a.tracker_store_testing_start ()
    print "   started, waiting 5 sec. to stop it"
    GLib.timeout_add_seconds (5, destroy_the_world, a)
    Gtk.main ()

    print "-- Starting miner-fs --"
    b = TrackerMinerFsLifeCycle ()
    b.start ()
    print "  started, waiting 3 secs. to stop it"
    time.sleep (3)
    b.stop ()
    print "  stopped"
