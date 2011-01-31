from optparse import OptionParser
import sys

usage = "usage: %prog [options]"

parser = OptionParser(usage=usage)
parser.add_option("-m", "--start-manually", dest="startmanually",
                  action="store_true",
                  default=False,
                  help="Wait for an external instance of the processes to appear in the system")

(options, args) = parser.parse_args()

# Deleting options from the args. Otherwise unittest will complain
for option in ["--startmanually", "-m"]:
    try:
        sys.argv.remove (option)
    except ValueError:
        pass

def is_manual_start ():
    """
    False to start the processes automatically
    """
    return options.startmanually
