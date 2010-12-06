#!/usr/bin/python
import os
import sys
import inspect
import imp

from common.utils import configuration as cfg

### This function comes from pydoc. Cool!
def importfile(path):
    """Import a Python source file or compiled file given its path."""
    magic = imp.get_magic()
    file = open(path, 'r')
    if file.read(len(magic)) == magic:
        kind = imp.PY_COMPILED
    else:
        kind = imp.PY_SOURCE
    file.close()
    filename = os.path.basename(path)
    name, ext = os.path.splitext(filename)
    file = open(path, 'r')
    module = None
    try:
        module = imp.load_module(name, file, path, (ext, 'r', kind))
    except Exception, e:
        print >> sys.stderr,  "Ignoring %s (%s)" % (path, e)
        #raise Exception ()
    file.close()
    return module


HEADER = """
<aegis>"""

FOOTER = """
</aegis>"""

def print_aegis_perm_request (filename):
    module = importfile (filename)
    if not module:
        return

    install_path = os.path.join (cfg.DATADIR, "tracker-tests", filename)

    print "\n   <request>"
    print '      <credential name="TrackerReadAccess" />'
    print '      <credential name="TrackerWriteAccess" />'
    print '      <credential name="tracker::tracker-extract-access" />' 
    print '      <credential name="tracker::tracker-miner-fs-access" />' 
    print '      <credential name="GRP::metadata-users" />' 
    print '      <for path="%s" />' % (install_path)
    print "   </request>"


if __name__ == "__main__":

    if (len (sys.argv) < 2):
        print >> sys.stderr, "pass .py tests as parameter"
        sys.exit (-1)
    print HEADER
    map (print_aegis_perm_request, sys.argv[1:])
    print FOOTER
