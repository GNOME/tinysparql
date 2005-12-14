/**
 * @file test/plugintest.c
 * @brief testcase for dynamic loading and unloading of plugins
 */
#include "platform.h"
#include "extractor.h"

int main(int argc, char * argv[]){
  int i;
  EXTRACTOR_ExtractorList * arg;
  EXTRACTOR_KeywordList * list;
  EXTRACTOR_KeywordList * list1;

  /* do some loading and unloading */
  for (i=0;i<10;i++) {
    arg = EXTRACTOR_loadDefaultLibraries();
    EXTRACTOR_removeAll(arg);
  }

  /* do some load/unload tests */
  arg = EXTRACTOR_addLibrary(NULL,
			     "libextractor_split");
  arg = EXTRACTOR_addLibrary(arg,
			     "libextractor_mime");
  arg = EXTRACTOR_addLibrary(arg,
			     "libextractor_filename");
  arg = EXTRACTOR_removeLibrary(arg,
				"libextractor_mime");
  arg = EXTRACTOR_removeLibrary(arg,
				"libextractor_split");
  arg = EXTRACTOR_removeLibrary(arg,
				"libextractor_filename");
  if (arg != NULL) {
    printf("add-remove test failed!\n");
    return -1;
  }

  arg = EXTRACTOR_addLibrary(NULL,
			     "libextractor_split");
  arg = EXTRACTOR_addLibrary(arg,
			     "libextractor_mime");
  arg = EXTRACTOR_addLibrary(arg,
			     "libextractor_filename");
  arg = EXTRACTOR_removeLibrary(arg,
				"libextractor_mime");
  arg = EXTRACTOR_removeLibrary(arg,
				"libextractor_filename");
  arg = EXTRACTOR_removeLibrary(arg,
				"libextractor_split");
  if (arg != NULL) {
    printf("add-remove test failed!\n");
    return -1;
  }

  arg = EXTRACTOR_loadConfigLibraries(NULL,
				      "libextractor_filename");
  arg = EXTRACTOR_loadConfigLibraries(arg,
				      "-libextractor_split");
  list = EXTRACTOR_getKeywords(arg,
			       "/etc/resolv.conf");
  if (3 != EXTRACTOR_countKeywords(list)) {
    printf("Invalid number of keywords (3 != %d)\n",
	   EXTRACTOR_countKeywords(list));
    return -1;
  }
  i = 0;
  list1 = list;
  while (list1 != NULL) {
    if ( (strcmp(list1->keyword, "resolv") == 0) ||
	 (strcmp(list1->keyword, "conf") == 0) ||
	 (strcmp(list1->keyword, "resolv.conf") == 0) )
      i++;
    list1 = list1->next;	
  }
  if (i != 3) {
    printf("Wrong keyword extracted.\n");
    EXTRACTOR_printKeywords(stderr,
			    list);
    return -1;
  }

  EXTRACTOR_removeAll(arg);
  EXTRACTOR_freeKeywords(list);

  return 0;
}
