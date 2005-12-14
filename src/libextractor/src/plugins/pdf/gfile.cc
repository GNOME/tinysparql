//========================================================================
//
// gfile.cc
//
// Miscellaneous file and directory name manipulation.
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#include "config.h"
#ifdef CYGWIN
#define WIN32 1
#endif
#ifndef WIN32
#include <pwd.h>
#endif
#include "GString.h"
#include "gfile.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// Some systems don't define this, so just make it something reasonably
// large.
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

//------------------------------------------------------------------------

GString *getHomeDir() {
#if defined(WIN32)
  //---------- OS/2+EMX and Win32 ----------
  char *s;
  GString *ret;

  if ((s = getenv("HOME")))
    ret = new GString(s);
  else
    ret = new GString(".");
  return ret;

#else
  //---------- Unix ----------
  char *s;
  struct passwd *pw;
  GString *ret;

  if ((s = getenv("HOME"))) {
    ret = new GString(s);
  } else {
    if ((s = getenv("USER")))
      pw = getpwnam(s);
    else
      pw = getpwuid(getuid());
    if (pw)
      ret = new GString(pw->pw_dir);
    else
      ret = new GString(".");
  }
  return ret;
#endif
}

GString *getCurrentDir() {
  char buf[PATH_MAX+1];

#if defined(WIN32)
  if (GetCurrentDirectory(sizeof(buf), buf))
#else
  if (getcwd(buf, sizeof(buf)))
#endif
    return new GString(buf);
  return new GString();
}

GString *appendToPath(GString *path, char *fileName) {
#if defined(WIN32)
  //---------- Win32 ----------
  GString *tmp;
  char buf[256];
  char *fp;

  tmp = new GString(path);
  tmp->append('/');
  tmp->append(fileName);
  GetFullPathName(tmp->getCString(), sizeof(buf), buf, &fp);
  delete tmp;
  path->clear();
  path->append(buf);
  return path;
#else
  //---------- Unix ----------
  int i;

  // appending "." does nothing
  if (!strcmp(fileName, "."))
    return path;

  // appending ".." goes up one directory
  if (!strcmp(fileName, "..")) {
    for (i = path->getLength() - 2; i >= 0; --i) {
      if (path->getChar(i) == '/')
	break;
    }
    if (i <= 0) {
      if (path->getChar(0) == '/') {
	path->del(1, path->getLength() - 1);
      } else {
	path->clear();
	path->append("..");
      }
    } else {
      path->del(i, path->getLength() - i);
    }
    return path;
  }

  // otherwise, append "/" and new path component
  if (path->getLength() > 0 &&
      path->getChar(path->getLength() - 1) != '/')
    path->append('/');
  path->append(fileName);
  return path;
#endif
}

GString *grabPath(char *fileName) {
#if defined(WIN32)
  //---------- OS/2+EMX and Win32 ----------
  char *p;

  if ((p = strrchr(fileName, '/')))
    return new GString(fileName, p - fileName);
  if ((p = strrchr(fileName, '\\')))
    return new GString(fileName, p - fileName);
  if ((p = strrchr(fileName, ':')))
    return new GString(fileName, p + 1 - fileName);
  return new GString();
#else
  //---------- Unix ----------
  char *p;

  if ((p = strrchr(fileName, '/')))
    return new GString(fileName, p - fileName);
  return new GString();
#endif
}

GBool isAbsolutePath(char *path) {
#if defined(WIN32)
  //---------- OS/2+EMX and Win32 ----------
  return path[0] == '/' || path[0] == '\\' || path[1] == ':';

#else
  //---------- Unix ----------
  return path[0] == '/';
#endif
}

GString *makePathAbsolute(GString *path) {
#if defined(WIN32)
  //---------- Win32 ----------
  char buf[_MAX_PATH];
  char *fp;

  buf[0] = '\0';
  if (!GetFullPathName(path->getCString(), _MAX_PATH, buf, &fp)) {
    path->clear();
    return path;
  }
  path->clear();
  path->append(buf);
  return path;
#else
  //---------- Unix and OS/2+EMX ----------
  struct passwd *pw;
  char buf[PATH_MAX+1];
  GString *s;
  char *p1, *p2;
  int n;

  if (path->getChar(0) == '~') {
    if (path->getChar(1) == '/' ||
	path->getLength() == 1) {
      path->del(0, 1);
      s = getHomeDir();
      path->insert(0, s);
      delete s;
    } else {
      p1 = path->getCString() + 1;
      for (p2 = p1; *p2 && *p2 != '/'; ++p2) ;
      if ((n = p2 - p1) > PATH_MAX)
	n = PATH_MAX;
      strncpy(buf, p1, n);
      buf[n] = '\0';
      if ((pw = getpwnam(buf))) {
	path->del(0, p2 - p1 + 1);
	path->insert(0, pw->pw_dir);
      }
    }
  } else if (!isAbsolutePath(path->getCString())) {
    if (getcwd(buf, sizeof(buf))) {
      path->insert(0, '/');
      path->insert(0, buf);
    }
  }
  return path;
#endif
}

time_t getModTime(char *fileName) {
#ifdef WIN32
  //~ should implement this, but it's (currently) only used in xpdf
  return 0;
#else
  struct stat statBuf;

  if (stat(fileName, &statBuf)) {
    return 0;
  }
  return statBuf.st_mtime;
#endif
}


GBool openTempFile(GString **name, FILE **f, char *mode, char *ext) {
#if defined(WIN32)
  //---------- non-Unix ----------
  char *s;

  // There is a security hole here: an attacker can create a symlink
  // with this file name after the tmpnam call and before the fopen
  // call.  I will happily accept fixes to this function for non-Unix
  // OSs.
  if (!(s = tmpnam(NULL))) {
    return gFalse;
  }
  *name = new GString(s);
  if (ext) {
    (*name)->append(ext);
  }
  if (!(*f = FOPEN((*name)->getCString(), mode))) {
    delete (*name);
    return gFalse;
  }
  return gTrue;
#else
  //---------- Unix ----------
  char *s, *p;
  int fd;

  if (ext) {
    if (!(s = tmpnam(NULL))) {
      return gFalse;
    }
    *name = new GString(s);
    s = (*name)->getCString();
    if ((p = strrchr(s, '.'))) {
      (*name)->del(p - s, (*name)->getLength() - (p - s));
    }
    (*name)->append(ext);
    fd = open((*name)->getCString(), O_WRONLY | O_CREAT | O_EXCL, 0600);
  } else {
#if HAVE_MKSTEMP
    if ((s = getenv("TMPDIR"))) {
      *name = new GString(s);
    } else {
      *name = new GString("/tmp");
    }
    (*name)->append("/XXXXXX");
    fd = mkstemp((*name)->getCString());
#else // HAVE_MKSTEMP
    if (!(s = tmpnam(NULL))) {
      return gFalse;
    }
    *name = new GString(s);
    fd = open((*name)->getCString(), O_WRONLY | O_CREAT | O_EXCL, 0600);
#endif // HAVE_MKSTEMP
  }
  if (fd < 0 || !(*f = fdopen(fd, mode))) {
    delete *name;
    return gFalse;
  }
  return gTrue;
#endif
}

//------------------------------------------------------------------------
// GDir and GDirEntry
//------------------------------------------------------------------------

GDirEntry::GDirEntry(char *dirPath, char *nameA, GBool doStat) {
#if defined(WIN32)
  int fa;
  GString *s;
#else
  struct stat st;
  GString *s;
#endif

  name = new GString(nameA);
  dir = gFalse;
  if (doStat) {
    s = new GString(dirPath);
    appendToPath(s, nameA);
#ifdef WIN32
    fa = GetFileAttributes(s->getCString());
    dir = (fa != 0xFFFFFFFF && (fa & FILE_ATTRIBUTE_DIRECTORY));
#else
    if (stat(s->getCString(), &st) == 0)
      dir = S_ISDIR(st.st_mode);
#endif
    delete s;
  }
}

GDirEntry::~GDirEntry() {
  delete name;
}

GDir::GDir(char *name, GBool doStatA) {
  path = new GString(name);
  doStat = doStatA;
#if defined(WIN32)
  GString *tmp;

  tmp = path->copy();
  tmp->append("/*.*");
  hnd = FindFirstFile(tmp->getCString(), &ffd);
  delete tmp;
#else
  dir = opendir(name);
#endif
}

GDir::~GDir() {
  delete path;
#if defined(WIN32)
  if (hnd) {
    FindClose(hnd);
    hnd = NULL;
  }
#else
  if (dir)
    closedir(dir);
#endif
}

GDirEntry *GDir::getNextEntry() {
  struct dirent *ent;
  GDirEntry *e;

  e = NULL;
#if defined(WIN32)
  e = new GDirEntry(path->getCString(), ffd.cFileName, doStat);
  if (hnd  && !FindNextFile(hnd, &ffd)) {
    FindClose(hnd);
    hnd = NULL;
  }
#else
  if (dir) {
    ent = readdir(dir);
    if (ent && !strcmp(ent->d_name, "."))
      ent = readdir(dir);
    if (ent)
      e = new GDirEntry(path->getCString(), ent->d_name, doStat);
  }
#endif
  return e;
}

void GDir::rewind() {
#ifdef WIN32
  GString *tmp;

  if (hnd)
    FindClose(hnd);
  tmp = path->copy();
  tmp->append("/*.*");
  hnd = FindFirstFile(tmp->getCString(), &ffd);
#else
  if (dir)
    rewinddir(dir);
#endif
}
