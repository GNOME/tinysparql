/**
 * @file test/trivialtest.c
 * @brief trivial testcase for libextractor plugin loading
 * @author Christian Grothoff
 */
#include "platform.h"
#include "extractor.h"

static int testLoadPlugins() {
  EXTRACTOR_ExtractorList * el;

  el = EXTRACTOR_loadDefaultLibraries();
  if (el == NULL) {
    printf("Failed to load default plugins!\n");
    return 1;
  }
  EXTRACTOR_removeAll(el);
  return 0;
}

int main(int argc, char * argv[]){
  int ret = 0;

  ret += testLoadPlugins();
  ret += testLoadPlugins();
  return ret;
}
