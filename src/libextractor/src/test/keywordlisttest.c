/**
 * @file test/keywordlisttest.c
 * @brief testcase for libextractor keyword list manipulation
 * @author Christian Grothoff
 */

#include "platform.h"
#include "extractor.h"

int main(int argc, char * argv[]){
  EXTRACTOR_KeywordType i;
  char * keywords[] = {
    "test", /* 0 */
    "me", /* 1 */
    "too", /* 2 */
    "many", /* 3 */
    "stupid", /* 0 */
    "silly", /* 1 */
    "keywords", /* 2 */
    "with", /* 3 */
    "too", /* 0 */
    "many", /* 1 */
    "repetitions", /* 2 */
    "many", /* 3 */
    "", /* 0 */
    NULL,
  };
  EXTRACTOR_KeywordList * head;
  EXTRACTOR_KeywordList * pos;

  head = NULL;
  i = 0;
  while (keywords[i] != NULL) {
    pos = (EXTRACTOR_KeywordList*) malloc(sizeof(EXTRACTOR_KeywordList));
    pos->next = head;
    pos->keywordType = i % 4;
    pos->keyword = strdup(keywords[i]);
    i++;
    head = pos;
  }
  if (0 != strcmp("test",
		  EXTRACTOR_extractLastByString(EXTRACTOR_getKeywordTypeAsString(0),
						head))) {
    printf("Wrong keyword returned by extractLastByString\n");
    return -1;
  }
  if (0 != strcmp("me",
		  EXTRACTOR_extractLast(1,
					head))) {
    printf("Wrong keyword returned by extractLast\n");
    return -1;
  }
  if (13 != EXTRACTOR_countKeywords(head)) {
    printf("Wrong number of keywords returned by countKeywords!\n");
    return -1;
  }
  head = EXTRACTOR_removeEmptyKeywords(head);
  if (12 != EXTRACTOR_countKeywords(head)) {
    printf("removeEmptyKeyword did not work!\n");
    return -1;
  }
  head = EXTRACTOR_removeDuplicateKeywords(head,
					   0);
  /* removes many - 3 */
  if (11 != EXTRACTOR_countKeywords(head)) {
    printf("removeDuplicateKeywords(0) did not work!\n");
    return -1;
  }

  head = EXTRACTOR_removeDuplicateKeywords(head,
					   EXTRACTOR_DUPLICATES_REMOVE_UNKNOWN);
  /* removes 0-too! */
  if (10 != EXTRACTOR_countKeywords(head)) {
    printf("removeDuplicateKeywords(2) did not work!\n");
    return -1;
  }

  head = EXTRACTOR_removeDuplicateKeywords(head,
					   EXTRACTOR_DUPLICATES_TYPELESS);
  /* removes many  */
  if (9 != EXTRACTOR_countKeywords(head)) {
    printf("removeDuplicateKeywords(1) did not work!\n");
    return -1;
  }
  EXTRACTOR_freeKeywords(head);
  return 0;
}
