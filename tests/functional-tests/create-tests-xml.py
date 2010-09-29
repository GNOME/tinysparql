#!/usr/bin/python2.6
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
<testdefinition version="0.1">
  <suite name="tracker">
    <description>Functional tests for the brilliant tracker</description> """

TEST_CASE_TMPL = """        <case name="%s">
            <description>%s</description>
            <step>%s</step>
        </case>"""

FOOTER = """
  </suite>
</testdefinition>"""

PRE_STEPS = """        <pre_steps>
           <step>pidof call-history|xargs kill -9</step>
           <step>initctl stop xsession/relevanced</step>
           <step>initctl stop xsession/tracker-miner</step>
           <step>su - user -c "tracker-control -k"</step>
       </pre_steps>
"""

def __get_doc (obj):
    if obj.__doc__:
        return obj.__doc__.strip ()
    else:
        return "FIXME description here"

def print_as_xml (filename):

    module = importfile (filename)
    if not module:
        return
    
    print "\n    <set name=\"%s\">" % (module.__name__)
    print "        <description>%s</description>" % (__get_doc (module))
    print PRE_STEPS
    for name, obj in inspect.getmembers (module):
        if (inspect.isclass (obj)
            and obj.__module__ == filename[:-3]):
            script = os.path.join (cfg.DATADIR, "tracker-tests", filename)
            print  TEST_CASE_TMPL % (name,
                                     __get_doc (obj),
                                     script + " " + name)

    print """        <environments>
            <scratchbox>true</scratchbox>
            <hardware>true</hardware>
        </environments>
    </set>
        """
    # Remove the compiled .pyc from the disk (because it looks ugly)
    #
    # First time a module is loaded, __file__ is the .py
    #  once the file is compiled, __file__ is .pyc
    if module.__file__.endswith (".py"):
        unlink = module.__file__ + "c"
    else:
        unlink = module.__file__
    os.unlink (unlink)


if __name__ == "__main__":

    if (len (sys.argv) < 2):
        print >> sys.stderr, "pass .py tests as parameter"
        sys.exit (-1)
    print HEADER
    map (print_as_xml, sys.argv[1:])
    print FOOTER
