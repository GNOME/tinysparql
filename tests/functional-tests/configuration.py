#!/usr/bin/python

# This is the configuration file for tracker testcases.
# Define the location of the test data in testDataDir
# This also checks if the program is running in target or in host and return the column of pid

import os, sys

testDataDir_pre = '/usr/share/tracker-func-ci-tests/testdata/'
testDataDir = '/home/user/MyDocs/.sounds/'

def dir_path():
	return testDataDir
		

def check_target():
	on_target = 'OSSO_PRODUCT_NAME'
	try :
	        if os.environ[on_target]:
		#if os.environ['DBUS_SESSION_BUS_ADDRESS'] == 'unix:path=/tmp/session_bus_socket':
               		awk_print = '1'
			return awk_print
	except KeyError:
	        awk_print = '2'
		return awk_print

'''
sboxindicator='/targets/links/scratchbox.config'
try :
        if os.path.islink(sboxindicator) and os.path.isfile(os.readlink(sboxindicator)) :
                awk_print = '3'
        else:
                awk_print = '2'

except OSError:
        awk_print = '2'
'''
	
