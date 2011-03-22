import subprocess
import os

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
        assert profile == "trackertest"
        # XDG_CONFIG_HOME is useless
        dconf_db = os.path.join (os.environ ["HOME"], ".config", "dconf", profile)
        print "[Conf] Removing dconf-profile:", dconf_db
        os.remove (dconf_db)


if __name__ == "__main__":


    SCHEMA_MINER = "org.freedesktop.Tracker.Miner.Files"
    os.environ ["DCONF_PROFILE"] = "trackertest"

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
