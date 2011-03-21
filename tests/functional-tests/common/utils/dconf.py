import subprocess

class DConfClient:
    """
    Shamefull implementation until we get GobjectIntrospection on libdconf
    """
    SCHEMA_MINER = "org.freedesktop.Tracker.Miner.FileSystem"
    
    def write (self, schema, key, value):
        if (type(value) == int):
            v = "i'%d'" % (value)
        else:
            v = value
        command = ["gsettings", "set", schema, key, v]
        print command
        FNULL = open('/dev/null', 'w')
        cmd = subprocess.Popen (command, stdout=subprocess.PIPE) #, stdout=FNULL, stderr=FNULL)
        cmd.wait ()

        
    def read (self, schema, key):
        command = ["gsettings", "get", schema, key]
        FNULL = open('/dev/null', 'w')
        cmd = subprocess.Popen (command, stdout=subprocess.PIPE) #, stdout=FNULL, stderr=FNULL)
        return cmd.stdout.readline ()


if __name__ == "__main__":

    dconf = DConfClient ()
    value = dconf.read (DConfClient.SCHEMA_MINER, "throttle")
    print "Original value:", int (value)
    print "Setting 5"
    dconf.write (DConfClient.SCHEMA_MINER, "throttle", 5)
    value = dconf.read (DConfClient.SCHEMA_MINER, "throttle")
    print "Current value:", int (value)
    print "Set 3"
    dconf.write (DConfClient.SCHEMA_MINER, "throttle", 3)
    value = dconf.read (DConfClient.SCHEMA_MINER, "throttle")
    print "Current value:", int (value)
        
