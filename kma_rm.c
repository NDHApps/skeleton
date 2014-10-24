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

typedef struct _header {
  int size;
  void *next;
} header_t;

/************Global Variables*********************************************/
static kma_page_t* head = NULL;

/************Function Prototypes******************************************/
void init_page(kma_page_t**);
void print_free_list();
void coalesce();
void attempt_to_free_pages();
/************External Declaration*****************************************/

/**************Implementation***********************************************/

void*
kma_malloc(kma_size_t size)
{
  if (size > PAGESIZE - sizeof(header_t)) return NULL;
  if (head == NULL) init_page(&head);
  if (DEBUG) {
    printf("\n    ---ALLOCATE %d---   \n", size);
    printf("Free list before allocation:\n");
    print_free_list();
  }
  header_t *curr = head->ptr;
  header_t *prev = NULL;
  while (curr != NULL) {
    if (curr->size > size + sizeof(header_t)) {
      if (DEBUG) printf("Allocating at %p\n", curr);
      // have to cast to char to ensure bytes are added correctly
      void *addr = (char*)curr + sizeof(header_t);
      header_t new_header = { .size = curr->size - size - sizeof(header_t),
                              .next = curr->next };
      assert(new_header.size >= 0);
      void *dest = (char*)curr + sizeof(header_t) + size;
      memcpy(dest, &new_header, sizeof(header_t));

      if (curr == head->ptr) {
        head->ptr = dest;
        if (DEBUG) printf("The head was moved to %p\n", dest);
      } else if (prev) {
        prev->next = dest;
        if (DEBUG) printf("Inserting in the middle of the list\n");
      }
      if (DEBUG) {
        printf("Free list after allocation:\n");
        print_free_list();
      }
      return addr;
    }
    prev = curr;
    curr = curr->next;
  }
  if (DEBUG) printf("Could not find spot for memory, allocating a new page\n");
  assert(prev->next == NULL);
  kma_page_t *new_page;
  init_page(&new_page);
  header_t *new_header = new_page->ptr;
  prev->next = new_header;
  if (DEBUG) { printf("Free list after allocating new page\n"); print_free_list(); }
  return (char*)new_header + sizeof(header_t);
}

void
kma_free(void* ptr, kma_size_t size)
{
  if (DEBUG) printf("\n    -----FREE %d-----   \n", size);
  header_t *freed, *curr, *prev;
  freed = ptr - sizeof(header_t);
  freed->size = size;
  if (DEBUG) printf("Freeing: %p - <%d, %p>\n", freed, freed->size, freed->next);
  curr = head->ptr;
  while (curr != NULL) {
    if (freed < curr) {
      if (DEBUG) printf("Inserting in front of %p\n", curr);
      if (curr == head->ptr) {
        if (DEBUG) printf("Reassigning the head!\n");
        head->ptr = freed;
        freed->next = curr;
      } else {
        if (DEBUG) printf("Inserting in the middle\n");
        prev->next = freed;
        freed->next = curr;
      }
      if (DEBUG) {
        printf("Free list after freeing\n");
        print_free_list();
      }
      coalesce();
      attempt_to_free_pages();
      return;
    }
    prev = curr;
    curr = curr->next;
  }
}

void
coalesce()
{
  if (DEBUG) printf("Coalescing\n");
  header_t *curr = head->ptr;
  while (curr->next != NULL)
  {
    if ((char*)curr + curr->size + sizeof(header_t) == curr->next) {
      if (DEBUG) printf("Coalescing %p and %p\n", curr, curr->next);
      curr->size += ((header_t*)(curr->next))->size + sizeof(header_t);
      curr->next =  ((header_t*)(curr->next))->next;
    } else {
      curr = curr->next;
    }
  }
  if (DEBUG) { printf("Free list after coalescing\n"); print_free_list(); }
}

void
attempt_to_free_pages()
{
  header_t *curr, *prev;
  prev = NULL;
  curr = head->ptr;
  while (curr != NULL) {
    if (curr->size == PAGESIZE - sizeof(header_t)) {
      if (DEBUG) printf("Found an an empty page, attemping to free it.\n");
      if (curr == head->ptr) {
        free_page(head);
      }
    }
    curr = curr->next;
  }
}

void
print_free_list()
{
  header_t *curr = head->ptr;
  while (curr != NULL) {
    printf("%p - <%d, %p>\n", curr, curr->size, curr->next);
    curr = curr->next;
  }
}

void
init_page(kma_page_t **page)
{
  printf("Initializing page\n");
  *page = get_page();
  header_t header = { .size = (PAGESIZE - sizeof(header_t)),
                      .next = NULL
                    };

  memcpy((*page)->ptr, &header, sizeof(header_t));
}

#endif // KMA_RM
