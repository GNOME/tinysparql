/*
     This file is part of libextractor.
     (C) 2004 Vidyut Samanta and Christian Grothoff

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
     option) any later version.

     libextractor is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with libextractor; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
 */

#include "platform.h"
#include "extractor.h"
#include "pack.h"

static void addKeyword(struct EXTRACTOR_Keywords ** list,
		       const char * keyword,
		       EXTRACTOR_KeywordType type) {
  EXTRACTOR_KeywordList * next;
  next = malloc(sizeof(EXTRACTOR_KeywordList));
  next->next = *list;
  next->keyword = strdup(keyword);
  next->keywordType = type;
  *list = next;
}

typedef unsigned int Elf32_Addr;
typedef unsigned short Elf32_Half;
typedef unsigned int Elf32_Off;
typedef signed int Elf32_Sword;
typedef unsigned int Elf32_Word;

/* first 4 bytes of the ELF header */
static char elfMagic[] = { 0x7f, 'E', 'L', 'F' };

#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6
#define EI_PAD 7
#define EI_NIDENT 16

typedef struct {
  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off e_phoff;
  Elf32_Off e_shoff; /* offset of the section header table */
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phensize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize; /* size of each entry in SH table */
  Elf32_Half e_shnum; /* how many entries in section header table */
  Elf32_Half e_shstrndx; /* section header's sh_name member is index into this string table! */
} Elf32_Ehdr;

/* elf-header minus e_ident */
#define ELF_HEADER_SIZE 36

#define ELF_HEADER_FIELDS(p) \
  &(p)->e_type,		     \
    &(p)->e_machine,	     \
    &(p)->e_version,	     \
    &(p)->e_entry,	     \
    &(p)->e_phoff,	     \
    &(p)->e_shoff,	     \
    &(p)->e_flags,	     \
    &(p)->e_ehsize,	     \
    &(p)->e_phensize,	     \
    &(p)->e_phnum,	     \
    &(p)->e_shentsize,	     \
    &(p)->e_shnum,	     \
    &(p)->e_shstrndx	
static char * ELF_HEADER_SPECS[] = {
  "hhwwwwwhhhhhh",
  "HHWWWWWHHHHHH",
};


typedef struct {
  Elf32_Word sh_name;
  Elf32_Word sh_type;
  Elf32_Word sh_flags;
  Elf32_Addr sh_addr; /* where loaded */
  Elf32_Off sh_offset; /* where in image (! sh_type==SHT_NOBITS) */
  Elf32_Word sh_size; /* section size in bytes */
  Elf32_Word sh_link;   /* for symbol table: section header index of the associated string table! */
  Elf32_Word sh_info; /* "one greater than the symbol table index of the last local symbol _STB_LOCAL_" */
  Elf32_Word sh_addralign;
  Elf32_Word sh_entsize;
} Elf32_Shdr;
#define ELF_SECTION_SIZE 40
#define ELF_SECTION_FIELDS(p) \
  &(p)->sh_name,	      \
    &(p)->sh_type,	      \
    &(p)->sh_flags,	      \
    &(p)->sh_addr,	      \
    &(p)->sh_offset,	      \
    &(p)->sh_size,	      \
    &(p)->sh_link,	      \
    &(p)->sh_info,	      \
    &(p)->sh_addralign,	      \
    &(p)->sh_entsize
static char * ELF_SECTION_SPECS[] = {
  "wwwwwwwwww",
  "WWWWWWWWWW",
};

typedef struct {
  Elf32_Word p_type;
  Elf32_Off p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
} Elf32_Phdr;
#define ELF_PDHR_SIZE 32
#define ELF_PHDR_FIELDS(p)	   \
  &(p)->p_type,			   \
    &(p)->p_offset,		   \
    &(p)->p_vaddr,		   \
    &(p)->p_paddr,		   \
    &(p)->p_filesz,		   \
    &(p)->p_memsz,		   \
    &(p)->p_flags,		   \
    &(p)->p_align
static char * ELF_PHDR_SPECS[] = {
  "wwwwwwww",
  "WWWWWWWW",
};

typedef struct {
  Elf32_Sword d_tag;
  union {
    Elf32_Word d_val;
    Elf32_Addr d_ptr;
  } d_un;
} Elf32_Dyn;
#define ELF_DYN_SIZE 8
#define ELF_DYN_FIELDS(p)			\
  &(p)->d_tag,					\
    &(p)->d_un
static char * ELF_DYN_SPECS[] = {
  "ww",
  "WW",
};

#define ET_NONE 0
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3
#define ET_CORE 4
#define ET_LOPROC 0xff00
#define ET_HIPROC 0xffff

#define EM_NONE 0
#define EM_M32 1
#define EM_SPARC 2
#define EM_386 3
#define EM_68K 4
#define EM_88K 5
#define EM_860 7
#define EM_MIPS 8

#define EV_NONE 0
#define EV_CURRENT 1

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
/* string table! */
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
/* dynamic linking info! */
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8
#define SHT_REL 9
#define SHT_SHLIB 10
#define SHT_DYNSYM 11
#define SHT_LOPROC 0x70000000
#define SHT_HIPROC 0x7fffffff
#define SHT_LOUSER 0x80000000
#define SHT_HIUSER 0xffffffff

#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4
#define SHF_MASKPROC 0xf000000

#define DT_NULL 0
/* name of a needed library, offset into table
   recorded in DT_STRTAB entry */
#define DT_NEEDED 1
#define DT_PLTRELSZ 2
#define DT_PLTGOT 3
#define DT_HASH 4
/* address of the string table from where symbol
   names, library names, etc for this DT come from */
#define DT_STRTAB 5
#define DT_SYMTAB 6
#define DT_SYMENT 7
#define DT_RELA 7
#define DT_RELASZ 8
#define DT_RELAENT 9
/* size of the string-table in bytes */
#define DT_STRSZ 10
/* fixme 11 */
#define DT_INIT 12
#define DT_FINI 13
/* string-table offset giving the name of the shared object */
#define DT_SONAME 14
/* string-table offset of a null-terminated library search path */
#define DT_RPATH 15
#define DT_SYMBOLIC 16


#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6
#define PT_LOPROC 0x70000000
#define PT_HIPROC 0x7fffffff




#define ELFCLASSNONE 0
#define ELFCLASS32 1
#define ELFCLASS64 2

#define ELFDATANONE 0
/* little endian */
#define ELFDATA2LSB 1
/* big endian */
#define ELFDATA2MSB 2

/**
 * @param ei_data ELFDATA2LSB or ELFDATA2MSB
 * @return 1 if we need to convert, 0 if not
 */
static int getByteorder(char ei_data) {
  if (ei_data == ELFDATA2LSB) {
#if __BYTE_ORDER == __BIG_ENDIAN
    return 1;
#else
    return 0;
#endif
  } else {
#if __BYTE_ORDER == __BIG_ENDIAN
    return 0;
#else
    return 1;
#endif
  }
}

/**
 *
 * @return 0 on success, -1 on error
 */
static int getSectionHdr(char * data,
			 size_t size,
			 Elf32_Ehdr * ehdr,
			 Elf32_Half idx,
			 Elf32_Shdr * ret) {
  if (ehdr->e_shnum <= idx)
    return -1;

  cat_unpack(&data[ehdr->e_shoff + ehdr->e_shentsize * idx],
	     ELF_SECTION_SPECS[getByteorder(data[EI_CLASS])],
	     ELF_SECTION_FIELDS(ret));
  return 0;
}

/**
 *
 * @return 0 on success, -1 on error
 */
static int getDynTag(char * data,
		     size_t size,
		     Elf32_Ehdr * ehdr,
		     Elf32_Off off,
		     Elf32_Word osize,
		     unsigned int idx,
		     Elf32_Dyn * ret) {
  if ( (off+osize > size) ||
       ((idx+1) * ELF_DYN_SIZE > osize) )
    return -1;
  cat_unpack(&data[off + idx*ELF_DYN_SIZE],
	     ELF_DYN_SPECS[getByteorder(data[EI_CLASS])],
	     ELF_DYN_FIELDS(ret));
  return 0;
}

/**
 *
 * @return 0 on success, -1 on error
 */
static int getProgramHdr(char * data,
			 size_t size,
			 Elf32_Ehdr * ehdr,
			 Elf32_Half idx,
			 Elf32_Phdr * ret) {
  if (ehdr->e_phnum <= idx)
    return -1;

  cat_unpack(&data[ehdr->e_phoff + ehdr->e_phensize * idx],
	     ELF_PHDR_SPECS[getByteorder(data[EI_CLASS])],
	     ELF_PHDR_FIELDS(ret));
  return 0;
}

/**
 * Parse ELF header.
 * @return 0 on success, -1 on error
 */
static int getELFHdr(char * data,
		     size_t size,
		     Elf32_Ehdr * ehdr) {
  /* catlib */
  if (size < sizeof(Elf32_Ehdr) + EI_NIDENT)
    return -1;
  if (0 != strncmp(data,
		   elfMagic,
		   sizeof(elfMagic)))
    return -1; /* not an elf */

  switch (data[EI_CLASS]) {
  case ELFDATA2LSB:
  case ELFDATA2MSB:
    cat_unpack(&data[EI_NIDENT],
	       ELF_HEADER_SPECS[getByteorder(data[EI_CLASS])],
	       ELF_HEADER_FIELDS(ehdr));
    break;
  default:
    return -1;
  }
  if (ehdr->e_shoff + ehdr->e_shentsize * ehdr->e_shnum > size)
    return -1; /* invalid offsets... */
  if (ehdr->e_shentsize < ELF_SECTION_SIZE)
    return -1; /* huh? */
  if (ehdr->e_phoff + ehdr->e_phensize * ehdr->e_phnum > size)
    return -1;

  return 0;
}

/**
 * @return the string (offset into data, do NOT free), NULL on error
 */
static const char * readStringTable(char * data,
				    size_t size,
				    Elf32_Ehdr * ehdr,
				    Elf32_Half strTableOffset,
				    Elf32_Word sh_name) {
  Elf32_Shdr shrd;
  if (-1 == getSectionHdr(data,
			  size,
			  ehdr,
			  strTableOffset,
			  &shrd))
    return NULL;
  if ( (shrd.sh_type != SHT_STRTAB) ||
       (shrd.sh_offset + shrd.sh_size > size) ||
       (shrd.sh_size <= sh_name) ||
       (data[shrd.sh_offset+shrd.sh_size-1] != '\0') )
    return NULL;
  return &data[shrd.sh_offset+sh_name];
}



/* application/x-executable, ELF */
struct EXTRACTOR_Keywords * libextractor_elf_extract(char * filename,
						     char * data,
						     size_t size,						
						     struct EXTRACTOR_Keywords * prev) {
  Elf32_Ehdr ehdr;
  Elf32_Half idx;

  if (0 != getELFHdr(data,
		     size,
		     &ehdr))
    return prev;
  addKeyword(&prev,
	     "application/x-executable",
	     EXTRACTOR_MIMETYPE);
  switch (ehdr.e_type) {
  case ET_REL:
    addKeyword(&prev,
	       "Relocatable file",
	       EXTRACTOR_RESOURCE_TYPE);
    break;
  case ET_EXEC:
    addKeyword(&prev,
	       "Executable file",
	       EXTRACTOR_RESOURCE_TYPE);
    break;
  case ET_DYN:
    addKeyword(&prev,
	       "Shared object file",
	       EXTRACTOR_RESOURCE_TYPE);
    break;
  case ET_CORE:
    addKeyword(&prev,
	       "Core file",
	       EXTRACTOR_RESOURCE_TYPE);
    break;
  default:
    break; /* unknown */
  }
  switch (ehdr.e_machine) {
  case EM_M32:
    addKeyword(&prev,
	       "M32",
	       EXTRACTOR_CREATED_FOR);
    break;
  case EM_386:
    addKeyword(&prev,
	       "i386",
	       EXTRACTOR_CREATED_FOR);
    break;
  case EM_68K:
    addKeyword(&prev,
	       "68K",
	       EXTRACTOR_CREATED_FOR);
    break;
  case EM_88K:
    addKeyword(&prev,
	       "88K",
	       EXTRACTOR_CREATED_FOR);
    break;
  case EM_SPARC:
    addKeyword(&prev,
	       "Sparc",
	       EXTRACTOR_CREATED_FOR);
    break;
  case EM_860:
    addKeyword(&prev,
	       "960",
	       EXTRACTOR_CREATED_FOR);
    break;
  case EM_MIPS:
    addKeyword(&prev,
	       "MIPS",
	       EXTRACTOR_CREATED_FOR);
    break;
  default:
    break; /* oops */
  }

  for (idx=0;idx<ehdr.e_phnum;idx++) {
    Elf32_Phdr phdr;

    if (0 != getProgramHdr(data,
			   size,
			   &ehdr,
			   idx,
			   &phdr))
      return prev;
    if (phdr.p_type == PT_DYNAMIC) {
      unsigned int dc = phdr.p_filesz / ELF_DYN_SIZE;
      unsigned int id;
      Elf32_Addr stringPtr;
      Elf32_Half stringIdx;
      Elf32_Half six;

      stringPtr = 0;

      for (id=0;id<dc;id++) {
	Elf32_Dyn dyn;
	if (0 != getDynTag(data,
			   size,
			   &ehdr,
			   phdr.p_offset,
			   phdr.p_filesz,
			   id,
			   &dyn))
	  return prev;
	if (DT_STRTAB == dyn.d_tag) {
	  stringPtr = dyn.d_un.d_ptr;
	  break;
	}
      }
      if (stringPtr == 0)
	return prev;
      for (six=0;six<ehdr.e_shnum;six++) {
	Elf32_Shdr sec;
	if (-1 == getSectionHdr(data,
				size,
				&ehdr,
				six,
				&sec))
	  return prev;
	if ( (sec.sh_addr == stringPtr) &&
	     (sec.sh_type == SHT_STRTAB) ) {
	  stringIdx = six;
	  break;
	}
      }

      for (id=0;id<dc;id++) {
	Elf32_Dyn dyn;
	if (0 != getDynTag(data,
			   size,
			   &ehdr,
			   phdr.p_offset,
			   phdr.p_filesz,
			   id,
			   &dyn))
	  return prev;
	switch(dyn.d_tag) {
	case DT_RPATH: {
	  const char * rpath;

	  rpath = readStringTable(data,
				  size,
				  &ehdr,
				  stringIdx,
				  dyn.d_un.d_val);
	  /* "source" of the dependencies: path
	     to dynamic libraries */
	  if (rpath != NULL) {
	    addKeyword(&prev,
		       rpath,
		       EXTRACTOR_SOURCE);
	  }
	  break;
	}
	case DT_NEEDED: {
	  const char * needed;

	  needed = readStringTable(data,
				  size,
				  &ehdr,
				  stringIdx,
				  dyn.d_un.d_val);
	  if (needed != NULL) {
	    addKeyword(&prev,
		       needed,
		       EXTRACTOR_DEPENDENCY);
	  }
	  break;
	}
	}
      }

    }
  }

  return prev;
}

