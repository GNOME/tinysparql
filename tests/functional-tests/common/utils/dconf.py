import subprocess
import os
from helpers import log

class DConfClient:
    """
    Shamefull implementation until we get GobjectIntrospection on libdconf
    """
    def write (self, schema, key, value):
        command = ["gsettings", "set", schema, key, str(value)]
        FNULL = open('/dev/null', 'w')
        cmd = subprocess.Popen (command, stdout=subprocess.PIPE) #, stdout=FNULL, stderr=FNULL)
        cmd.wait ()

        
    def read (self, schema, key):
        command = ["gsettings", "get", schema, key]
        FNULL = open('/dev/null', 'w')
        cmd = subprocess.Popen (command, stdout=subprocess.PIPE) #, stdout=FNULL, stderr=FNULL)
        return cmd.stdout.readline ()

    def reset (self):
        profile = os.environ ["DCONF_PROFILE"]
        if not os.path.exists(profile):
            raise Exception(
                "Unable to find DConf profile '%s'. Check that Tracker and "
                "the test suite have been correctly installed (you must pass "
                "--enable-functional-tests to configure)." % profile)

        assert os.path.basename(profile) == "trackertest"

        # XDG_CONFIG_HOME is useless, so we use HOME. This code should not be
        # needed unless for some reason the test is not being run via the
        # 'test-runner.sh' script.
        dconf_db = os.path.join (os.environ ["HOME"],
                                 ".config",
                                 "dconf",
                                 "trackertest")
        if os.path.exists (dconf_db):
            log ("[Conf] Removing dconf database: " + dconf_db)
            os.remove (dconf_db)


if __name__ == "__main__":


    SCHEMA_MINER = "org.freedesktop.Tracker.Miner.Files"
    os.environ ["DCONF_PROFILE"] = os.path.join (cfg.DATADIR, "tracker-tests",
                                                 "trackertest")

    dconf = DConfClient ()
    value = dconf.read (DConfClient.SCHEMA_MINER, "throttle")
    print "Original value:", int (value)
    print "Setting 5"
    dconf.write (DConfClient.SCHEMA_MINER, "throttle", "5")
    value = dconf.read (DConfClient.SCHEMA_MINER, "throttle")
    assert int(value) == 5
    
    print "Setting 3"
    dconf.write (DConfClient.SCHEMA_MINER, "throttle", "3")
    value = dconf.read (DConfClient.SCHEMA_MINER, "throttle")
    assert int (value) == 3

    print "Now with lists"
    dconf.write (DConfClient.SCHEMA_MINER, "index-recursive-directories", ['$HOME/set-with-python'])
    value = dconf.read (DConfClient.SCHEMA_MINER, "index-recursive-directories")
    print "result", value
