/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the resource map algorithm
 *    Author: Stefan Birrer
 *    Copyright: 2004 Northwestern University
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    Revision 1.2  2009/10/31 21:28:52  jot836
 *    This is the current version of KMA project 3.
 *    It includes:
 *    - the most up-to-date handout (F'09)
 *    - updated skeleton including
 *        file-driven test harness,
 *        trace generator script,
 *        support for evaluating efficiency of algorithm (wasted memory),
 *        gnuplot support for plotting allocation and waste,
 *        set of traces for all students to use (including a makefile and README of the settings),
 *    - different version of the testsuite for use on the submission site, including:
 *        scoreboard Python scripts, which posts the top 5 scores on the course webpage
 *
 *    Revision 1.1  2005/10/24 16:07:09  sbirrer
 *    - skeleton
 *
 *    Revision 1.2  2004/11/05 15:45:56  sbirrer
 *    - added size as a parameter to kma_free
 *
 *    Revision 1.1  2004/11/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *
 ***************************************************************************/
#ifdef KMA_RM
#define __KMA_IMPL__
#define DEBUG 1

#define PAIR_LIST_LENGTH 30

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

typedef struct pair {
  int isFree;
  int base;
  int size;
} pair_t;

typedef struct header {
  kma_page_t *self;
  kma_page_t *next;
  int len;
  void *start;
  pair_t free_list[PAIR_LIST_LENGTH];
} header_t;

/************Global Variables*********************************************/
static kma_page_t *store = NULL;

/************Function Prototypes******************************************/
void init_store();
void print_header();
int comp_pair(const void*, const void*);
/************External Declaration*****************************************/

/**************Implementation***********************************************/

void*
kma_malloc(kma_size_t size)
{
  if (DEBUG) printf("\n---ALLOCATE %d---\n", size);
  if (store == NULL)
    init_store();
  if (DEBUG) print_header();

  // search for open spot
  header_t *header = store->ptr;
  int i;
  pair_t pair;
  for(i=0;i<header->len;i++) {
    pair = header->free_list[i];
    if (pair.isFree && pair.size >= size) {
      if (DEBUG) printf("Allocating %d bytes at %d\n", size, pair.base);
      void* addr = header->start + pair.base;
      header->free_list[i].base += size;
      header->free_list[i].size -= size;
      if (DEBUG) print_header();
      return addr;
    }
  }
  return NULL;
}

void
kma_free(void* ptr, kma_size_t size)
{
  if (DEBUG) printf("\n---FREE %p, %d---\n", ptr, size);
  header_t *header = store->ptr;
  int base = (int)((ptr - header->start) / (sizeof(void*)));
  if (DEBUG) printf("Freeing at %d", base);
  pair_t new_pair = { .isFree = 1, .base = base, .size = size };
  header->free_list[header->len] = new_pair;
  header->len++;
  qsort(&header->free_list, header->len, sizeof(pair_t), comp_pair);
  if (DEBUG) print_header();
}

void print_header()
{
  header_t *header = store->ptr;
  printf("\n###HEADER of %p###\n", store->ptr);
  printf("SELF: %p\n", header->self);
  if (header->next) printf("NEXT: %p\n", header->next);
  printf("LEN: %d\n", header->len);
  printf("START: %p\n", header->start);
  int k;
  pair_t pair;
  printf("FREE LIST:\n");
  for(k=0;k < header->len;k++) {
    pair = header->free_list[k];
    printf("-- BASE: %d, LENGTH: %d, FREE: %d\n", pair.base, pair.size, pair.isFree);
  }
}

void init_store()
{
  if (DEBUG) printf("Initializing store\n");
  store = get_page();
  void *page = store->ptr;

  if (DEBUG) printf("Initializing header\n");
  header_t info = {
    .self = store,
    .next = NULL,
    .len = 1,
    .start = page + sizeof(header_t)
  };

  int i;
  if (DEBUG) printf("Initializing base pairs\n");
  pair_t temp = { .isFree = 0, .base = 0, .size = 0 };
  for (i=0;i<PAIR_LIST_LENGTH;i++) {
    info.free_list[i] = temp;
  }
  if (DEBUG) printf("Initializing first base pair\n");
  info.free_list[0].isFree = 1;
  info.free_list[0].base = 0;
  info.free_list[0].size = PAGESIZE - sizeof(header_t);

  memcpy(page, &info, sizeof(header_t));
}

int comp_pair(const void* p1, const void* p2)
{
  pair_t *pair1 = (pair_t*)p1;
  pair_t *pair2 = (pair_t*)p2;

  if (!pair1->isFree && !pair2->isFree) return 0;
  if (!pair1->isFree) return -1;
  if (!pair2->isFree) return 1;

  if (pair1->base > pair2->base) return 1;
  else if (pair1->base < pair2->base) return -1;
  else {
    if (DEBUG) printf("Something's up, two pairs have the same base!\n");
    return 0;
  }
}

#endif // KMA_RM
