#!/usr/bin/env python
#    Created by Eugenio Cutolo  me at eugesoftware dot com
#
#    This program can be distributed under the terms of the GNU GPL.
#    See the file COPYING.
#

from fuse import Fuse
from optparse import OptionParser
from errno import *
from stat import *
import os,statvfs,logging,dbus,re

#Simple class to interface with tracker dbus function
class TrackerClient:

	def __init__(self):
		#Initialize dbus session and tracker interfaces
		bus = dbus.SessionBus()
		obj = bus.get_object('org.freedesktop.Tracker','/org/freedesktop/tracker')
		
		self.tracker_iface = dbus.Interface(obj, 'org.freedesktop.Tracker')
		self.keywords_iface = dbus.Interface(obj, 'org.freedesktop.Tracker.Keywords')
		self.search_iface = dbus.Interface(obj, 'org.freedesktop.Tracker.Search')
		self.files_iface = dbus.Interface(obj, 'org.freedesktop.Tracker.Files')
		
		self.version = self.tracker_iface.GetVersion()
		self.query_id = -1

	def search(self,text,service='Files',offset=0,max_hits=-1):
		self.resultlist = self.search_iface.Text(self.query_id, service, text, offset, max_hits)
		self.resultlist = map(lambda x: str(x),self.resultlist)
		
	def search_by_tag(self,tag,service='Files',offset=0,max_hits=-1):
		self.resultlist = self.keywords_iface.Search(self.query_id, service,tag,offset,max_hits)
		self.resultlist = map(lambda x: str(x),self.resultlist)

	def add_db_files(self,path):
		self.files_iface.Exists(path,True)
		self.files_iface.Exists(path,False)
		return True

	def add_tag(self,path,tags,service='Files'):
		self.keywords_iface.Add(service,path,tags)
		return

	#In future will be implemented live_query support
	def on_tracker_reply(self, results):
		print results
		
	def on_tracker_error(self, e):
		print "Error:"+e

#This class it's an extension of Fuse and TrackerClient
class TrackerFs (Fuse,TrackerClient):

	def __init__(self):
		#Initialize tracker client
		TrackerClient.__init__(self)
		
		#Shell inteface
		usage = "usage: %prog mountpoint [options]"
		
		self.parser = OptionParser(usage)
		self.parser.add_option("-a", "--auto",action="store_true",help="Mount point will be populated with the rdf query in ~/.Tracker/fs", dest="automatic", default=True)
		self.parser.add_option("-s", "--search", dest="keys",help="Use a key to find the contents of mounted dir", metavar="key")
		self.parser.add_option("-t", "--tag",dest="tag",help="Use a tag/s to find the contents of mounted dir", metavar="tag")
		self.parser.add_option("-q", "--query",dest="query",help="Use a rdf file to find the contents of mounted dir", metavar="path")
		self.parser.add_option("-v", "--verbose",action="store_true",help="Verbose the output", dest="verbose", default=False)
		self.params, args = self.parser.parse_args()

		if os.path.exists(args[0]) == False:
			print "The mount point doesen't exist make it?"
			#self.log.debug("Create target directory")
			os.mkdir(args[0])		

		#check old files
		#save_dir = open(args[0], O_RDONLY);
		#fchdir(save_dir);
		#close(save_dir);

		#Init fuse
		Fuse.__init__(self,args,{})

		if self.params.verbose:
			verbmode = logging.DEBUG
		else:
			verbmode = logging.WARNING
		
		#Setup logger
		self.log = logging.getLogger("trackerfs");self.log.setLevel(verbmode)
		fh = logging.StreamHandler();fh.setLevel(verbmode)
		formatter = logging.Formatter("%(asctime)s - %(name)s - %(levelname)s - %(message)s")
		fh.setFormatter(formatter)
		self.log.addHandler(fh)
		
		#This is the path when file will be "really" created/moved/edited
		self.realdir = "/media/Dati/.data/"
		self.rdfdir = os.environ["HOME"]+"/.Tracker/fs/"

		if os.path.exists(self.realdir) == False:
			self.log.debug("Create target directory")
			os.mkdir(self.realdir)

		self.log.debug("mountpoint: %s" % repr(self.mountpoint))

		#Get list of rdf query
		#files = os.listdir(self.rdfdir)
		#self.queryfiles = [f for f in files if f[-4:]]
		#print self.queryfiles
		self._refresh_filelist()
		pass

	def mythread(self):
		self.log.debug("Start Thread")


#Refresh file list calling the TrackerClient function
	def _refresh_filelist(self):
		if self.params.tag != None:
			self.log.debug("Use Tag")
			self.search_by_tag(self.params.tag.split("+"))
		#Command Line support for query soon will be move to d-bus
		elif self.params.query != None:
			self.log.debug("Use Query")
			self.resultlist = os.popen("/usr/bin/tracker-query "+self.params.query,"r").readlines();
			self.resultlist = map(lambda x:(x.split(' : ')),self.resultlist)
			self.resultlist = map(lambda x:(x[0].strip(' \t\r\n')),self.resultlist)
		elif self.params.keys != None:
			self.log.debug("Use Search")
			self.search(self.params.keys)#.search,self.params.service)
		else:
			print 'You must specify an options'
			return 0
		self.log.debug("Refresh Filelist")

	def _get_file_path(self,filename):
		if os.path.dirname(filename) == "/":
			for file in self.resultlist:
				if filename== "/"+os.path.basename(file):
					return file
		else:
			path = filename.split("/")
			relpath = filename.replace("/"+path[1],"")
			for file in self.resultlist:
				if path[1] == os.path.basename(file):
					return file+relpath
		return filename

	def create_file_or_dir(self,type,path,mode):
		if os.path.dirname(path) == "/" and self.params.tag != None:
			newpath = self.realdir+os.path.basename(path)
		elif os.path.dirname(path) != "/":
			newpath = self._get_file_path(path)
		else:
			self.log.error("Fs based on Search and Query doesen't have write access")
			return 0
		if S_ISREG(mode) and type == 0:
			self.log.debug("MkFile:"+newpath)
			res = os.open(newpath, os.O_CREAT | os.O_WRONLY,mode);
		elif type == 1:	
			self.log.debug("MkDir:"+newpath)
			res = os.mkdir(newpath,mode)
		else:
			return -EINVAL
		print "path:"+os.path.dirname(path)
		if os.path.dirname(path) == "/":
			print "Add to fs"
			self.add_to_fs(newpath)
		self._refresh_filelist()
		self.getdir("/")
		return res

#Add file/dir to the tracker database and tag it with used tag
	def add_to_fs(self,path):
		if self.add_db_files(path):
			self.log.debug("Added "+os.path.basename(path)+" to tracker database")
			self.add_tag(path,self.params.tag.split("+"))
			self.log.debug("Added "+os.path.basename(path)+" to fs")
		else:
			self.log.debug("Falied to add "+path+"to tracker database")

	def getattr(self,path):
		self.log.debug("GetAttr:"+self._get_file_path(path))
		return os.lstat(self._get_file_path(path))

	def readlink(self, path):
		self.log.debug("ReadLink:"+self._get_file_path(path))
		return os.readlink(self._get_file_path(path))

	def getdir(self, path):
		if path == "/":
			self._refresh_filelist()
			return map(lambda x: (os.path.basename(x),0),self.resultlist)
		else:
			return map(lambda x: (os.path.basename(x),0),os.listdir(self._get_file_path(path)))


	def unlink(self, path):
		return os.unlink(path)

	def rmdir(self, path):
		return os.rmdir(path)

	def symlink(self, path, path1):
		return os.symlink(path, path1)

	def rename(self, path, path1):
		return os.rename(path, path1)

	def link(self, path, path1):
		return os.link(path, path1)

	def chmod(self, path, mode):
		return os.chmod(self._get_file_path(path), mode)

	def chown(self, path, user, group):
		return os.chown(self._get_file_path(path), user, group)

	def truncate(self, path, size):
		f = open(self._get_file_path(path), "w+")
		return f.truncate(size)

	def mknod(self, path, mode, dev):
		res = self.create_file_or_dir(0,path,mode)

	def mkdir(self, path, mode):
		return self.create_file_or_dir(1,path,mode)

	def utime(self, path, times):
		return os.utime(self._get_file_path(path), times)

	def open(self, path, flags):
		path = self._get_file_path(path)
		res = os.open(path,flags)
		self.log.debug("open"+path)
		os.close(res)
		return 0

	def read(self, path, length, offset):
		path = self._get_file_path(path)
		self.log.debug("read:"+ path)
		f = open(self._get_file_path(path), "r")
		f.seek(offset)
		return f.read(length)

	def write(self, path, buf, off):
		path = self._get_file_path(path)
		self.log.debug(":write:"+path)
		f = open(path, "w")
		f.seek(off)
		f.write(buf)
		return len(buf)
		return 0

	def release(self, path, flags):
		self.log.debug("release: %s %s" % (self._get_file_path(path), flags))
		return 0

	def statfs(self):
		self.sysconst = os.statvfs(self.realdir)
		self.log.debug("statfs: returning user's home value ")
		block_size = self.sysconst[statvfs.F_BSIZE]
		blocks = self.sysconst[statvfs.F_BLOCKS]
		blocks_free = self.sysconst[statvfs.F_BFREE]
		blocks_avail = self.sysconst[statvfs.F_BAVAIL]
		files = self.sysconst[statvfs.F_FILES]
		files_free = self.sysconst[statvfs.F_FFREE]
		namelen = self.sysconst[statvfs.F_NAMEMAX]
		return (block_size, blocks, blocks_free, blocks_avail, files, files_free, namelen)

	def fsync(self, path, isfsyncfile):
		self.log.debug("fsync: path=%s, isfsyncfile=%s" % (path, isfsyncfile))
		return 0

if __name__ == '__main__':	
	fs = TrackerFs()
	fs.multithreaded = 0;
	fs.main()
