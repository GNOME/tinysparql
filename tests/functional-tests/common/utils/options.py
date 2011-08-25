from optparse import OptionParser
import sys

usage = "usage: %prog [options]"

parser = OptionParser(usage=usage)

parser.add_option("-m", "--start-manually", dest="startmanually",
                  action="store_true",
                  default=False,
                  help="Wait for an external instance of the processes to appear in the system")

parser.add_option("-v", "--verbose", dest="verbose",
                  action="store_true",
                  default=False,
                  help="Display a log of test process statuses")

(options, args) = parser.parse_args()

# Deleting options from the args. Otherwise unittest and the tests which
# have their own simple commandline parsers will complain
for option in ["--startmanually", "-m", "--verbose", "-v"]:
    try:
        sys.argv.remove (option)
    except ValueError:
        pass

def is_verbose ():
    """
    True to log process status information to stdout
    """
    return options.verbose

def is_manual_start ():
    """
    False to start the processes automatically
    """
    return options.startmanually
