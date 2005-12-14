/**
 * @file test/multiload.c
 * @brief testcase for libextractor plugin loading that loads the same
 *    plugins multiple times!
 * @author Christian Grothoff
 */

#include "platform.h"
#include "extractor.h"

static int testLoadPlugins() {
  EXTRACTOR_ExtractorList * el1;
  EXTRACTOR_ExtractorList * el2;

  el1 = EXTRACTOR_loadDefaultLibraries();
  el2 = EXTRACTOR_loadDefaultLibraries();
  if ( (el1 == NULL) || (el2 == NULL) ) {
    printf("Failed to load default plugins!\n");
    return 1;
  }
  EXTRACTOR_removeAll(el1);
  EXTRACTOR_removeAll(el2);
  return 0;
}

int main(int argc, char * argv[]){
  int ret = 0;

  ret += testLoadPlugins();
  ret += testLoadPlugins();
  return ret;
}
