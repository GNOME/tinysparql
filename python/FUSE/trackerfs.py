#!/usr/bin/env python
#    This program can be distributed under the terms of the GNU GPL.

from fuse import Fuse
from optparse import OptionParser
from errno import *
from stat import *
import os,statvfs,thread,logging,dbus


# Set up logging
log = logging.getLogger("trackerfs")
log.setLevel(logging.DEBUG)
fh = logging.StreamHandler()
fh.setLevel(logging.DEBUG)
#create formatter
formatter = logging.Formatter("%(asctime)s - %(name)s - %(levelname)s - %(message)s")
fh.setFormatter(formatter)
log.addHandler(fh)

class TrackerFs (Fuse):

	def __init__(self,*args, **kw):
		Fuse.__init__(self,*args,**kw)
		
		#Initialize dbus session and tracker interfaces
		bus = dbus.SessionBus()
		obj = bus.get_object('org.freedesktop.Tracker','/org/freedesktop/tracker')
		
		self.tracker = dbus.Interface(obj, 'org.freedesktop.Tracker')
		self.keywords = dbus.Interface(obj, 'org.freedesktop.Tracker.Keywords')
		self.search = dbus.Interface(obj, 'org.freedesktop.Tracker.Search')
		self.filesdb = dbus.Interface(obj, 'org.freedesktop.Tracker.Files')
		
		log.debug("mountpoint: %s" % repr(self.mountpoint))
		log.debug("unnamed mount options: %s" % self.optlist)
		log.debug("named mount options: %s" % self.optdict)
		
		self.filelist = []
		self.tmplist = []
		self._refresh_filelist()
		pass

	def mythread(self):
		log.debug("Start Thread")
		#In future this thread refresh the filelist and tmplist


	def _refresh_filelist(self):
		if self.optdict.has_key("tag"):
			log.debug("Use Tag")
			self.filelist = self.keywords.Search(-1,'Files',self.optdict['tag'].split("+"),-1,-1)
			self.filelist = map(lambda x: str(x),self.filelist)
		#Command Line support for query soon will be move to d-bus
		elif self.optdict.has_key("query"):
			log.debug("Use Query")
			self.filelist = os.popen("/usr/bin/tracker-query "+self.optdict['query'],"r").readlines();
			self.filelist = map(lambda x:(x.split(' : ')),self.filelist)
			self.filelist = map(lambda x:(x[0].strip(' \t\r\n')),self.filelist)
		elif self.optdict.has_key("search"):
			log.debug("Use Search")
			self.filelist = self.search.Text(-1, "Files",self.optdict['search'], 0, 512, False)
			self.filelist = map(lambda x: str(x),self.filelist)
		else:
			print 'You must specify a tag or a query'
			return 0
		self.filelist.extend(self.tmplist)
		log.debug("Refresh Filelist")
		#self.filelist.pop()

	def _get_file_path(self,filename):
		if os.path.dirname(filename) == "/":
			for file in self.filelist:
				if filename== "/"+os.path.basename(file):
					return file
		else:
			path = filename.split("/")
			relpath = filename.replace("/"+path[1],"")
			for file in self.filelist:
				if path[1] == os.path.basename(file):
					return file+relpath
		return filename

	def getattr(self,path):
		log.debug("GetAttr:"+self._get_file_path(path))
		return os.lstat(self._get_file_path(path))

	def readlink(self, path):
		log.debug("ReadLink:"+self._get_file_path(path))
		return os.readlink(self._get_file_path(path))

	def getdir(self, path):
		if path == "/":
			self._refresh_filelist()
			return map(lambda x: (os.path.basename(x),0),self.filelist)
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

#For now when you move or copy a file in the mounted dir it will be moved
#to ~/.documents dir and the path will be added to the tmplist

	def mknod(self, path, mode, dev):
		if os.path.dirname(path) == "/":
			path = os.environ["HOME"]+"/.documents/"+os.path.basename(path)		
			if S_ISREG(mode):
				self.tmplist.append(path)
				self._refresh_filelist()
				self.getdir("/")
				res = os.open(path, os.O_CREAT | os.O_WRONLY,mode);
				log.debug("MkNod:"+path)
				#restat = os.fstat(res);
				#self.filesdb.Create(path,False,'image/png',dbus.Int32(restat[ST_SIZE]),restat[ST_MTIME])
			else:
				return -EINVAL
#See over

	def mkdir(self, path, mode):
		path = os.environ["HOME"]+"/.documents/"+os.path.basename(path)
		res = os.mkdir(path,mode)
		self.tmplist.append(path)
		self._refresh_filelist()
		self.getdir("/")
		log.debug("MkDir:"+path)
		return res

	def utime(self, path, times):
		return os.utime(self._get_file_path(path), times)

	def open(self, path, flags):
		path = self._get_file_path(path)
		res = os.open(path,flags)
		log.debug("open"+path)
		os.close(res)
		return 0

	def read(self, path, length, offset):
		path = self._get_file_path(path)
		log.debug("read:"+ path)
		f = open(self._get_file_path(path), "r")
		f.seek(offset)
		return f.read(length)

	def write(self, path, buf, off):
		path = self._get_file_path(path)
		log.debug(":write:"+path)
		f = open(path, "w")
		f.seek(off)
		f.write(buf)
		return len(buf)
		return 0

	def release(self, path, flags):
		log.debug("release: %s %s" % (self._get_file_path(path), flags))
		return 0

	def statfs(self):
		self.sysconst = os.statvfs(os.environ["HOME"])
		log.debug(":statfs: returning user's home value ")
		block_size = self.sysconst[statvfs.F_BSIZE]
		blocks = self.sysconst[statvfs.F_BLOCKS]
		blocks_free = self.sysconst[statvfs.F_BFREE]
		blocks_avail = self.sysconst[statvfs.F_BAVAIL]
		files = self.sysconst[statvfs.F_FILES]
		files_free = self.sysconst[statvfs.F_FFREE]
		namelen = self.sysconst[statvfs.F_NAMEMAX]
		return (block_size, blocks, blocks_free, blocks_avail, files, files_free, namelen)

	def fsync(self, path, isfsyncfile):
		log.debug("fsync: path=%s, isfsyncfile=%s" % (path, isfsyncfile))
		return 0

if __name__ == '__main__':
	server = TrackerFs()
	server.multithreaded = 0;
	server.main()
