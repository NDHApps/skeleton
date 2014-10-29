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
#define DEBUG 0

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
static kma_page_t* entry = NULL;

/************Function Prototypes******************************************/
void init_page(kma_page_t**);
void print_free_list();
void coalesce();
void attempt_to_free_pages();
void check_list();
header_t* get_head();
void move_head(header_t**);
/************External Declaration*****************************************/

/**************Implementation***********************************************/

header_t* get_head()
{
  return *(header_t**)(BASEADDR(entry->ptr));
}

void move_head(header_t** dest)
{
  memcpy(BASEADDR(entry->ptr), dest, sizeof(header_t*));
}

void*
kma_malloc(kma_size_t size)
{
  if (size > PAGESIZE - sizeof(header_t)) return NULL;
  if (entry == NULL) {
    kma_page_t *page = get_page();
    init_page(&page);

    header_t* head = (header_t*)(page->ptr + sizeof(kma_page_t*));
    if (DEBUG) printf("Initializing entry with first header\n");
    entry = get_page();
    move_head(&head);
  }

  if (DEBUG) {
    printf("\n    ---ALLOCATE %d---   \n", size);
    printf("Free list before allocation:\n");
    print_free_list();
  }

  header_t* head = get_head();

  if (DEBUG) printf("First header, <%d, %p>\n", head->size, head->next);

  header_t *curr = get_head();
  header_t *prev = NULL;
  while (curr != NULL) {
    if (curr->size >= size + sizeof(header_t)) {
      if (DEBUG) printf("Allocating at %p\n", curr);
      // have to cast to char to ensure bytes are added correctly
      void *addr = (char*)curr + sizeof(header_t);

      header_t new_header = {
        .size = curr->size - size - sizeof(header_t),
        .next = curr->next
      };

      assert(new_header.size >= 0);
      assert(new_header.size <= (PAGESIZE - sizeof(header_t) - sizeof(void*)));
      void *dest = (char*)curr + sizeof(header_t) + size;
      memcpy(dest, &new_header, sizeof(header_t));

      if (curr == get_head()) {
        move_head((header_t**)&dest);
        if (DEBUG) printf("The head was moved to %p\n", dest);
      } else if (prev) {
        prev->next = dest;
        if (DEBUG) printf("Inserting in the middle of the list\n");
      }
      if (DEBUG) {
        printf("Free list after allocation:\n");
        print_free_list();
      }
      if (DEBUG) { printf("Checking sanity of list after alloc\n"); check_list(); }
      return addr;
    }
    prev = curr;
    curr = curr->next;
  }
  if (DEBUG) printf("Could not find spot for memory, allocating a new page\n");
  assert(prev->next == NULL);
  kma_page_t *new_page = get_page();
  init_page(&new_page);

  void* addr = new_page->ptr + sizeof(void*) + sizeof(header_t);
  if (DEBUG) printf("Reassinging new header\n");
  void* dest = new_page->ptr + sizeof(void*) + size + sizeof(header_t);
  header_t new_header = {
    .size = (PAGESIZE) - size - sizeof(header_t)*2 - sizeof(void*),
    .next = NULL
  };
  memcpy(dest, &new_header, sizeof(header_t));
  header_t *newh = (header_t*)dest;
  prev->next = newh;

  if (DEBUG) {
    printf("Created a new header at %p, <%d, %p>\n", newh, newh->size, newh->next);
    printf("Free list after allocation:\n");
    print_free_list();
  }

  return addr;
}

void
kma_free(void* ptr, kma_size_t size)
{
  if (DEBUG) {
    printf("\n   ---FREE %p, %d---   \n", ptr, size);
    if (DEBUG) printf("Free list before free\n");
    print_free_list();
  }
  header_t *freed, *curr;
  header_t *prev = NULL;

  freed = ptr - sizeof(header_t);
  freed->size = size;
  if (DEBUG) printf("Freeing: %p - <%d, %p>\n", freed, freed->size, freed->next);
  curr = get_head();
  while (curr != NULL) {
    if (BASEADDR(freed) == BASEADDR(curr)) {
      if (freed < curr) {
        if (DEBUG) printf("Inserting in front of %p\n", curr);
        if (curr == get_head()) {
          if (DEBUG) printf("Reassigning the head!\n");
          move_head(&freed);
        } else {
          if (DEBUG) printf("Inserting in the middle, linking %p to %p\n", prev, freed);
          prev->next = freed;
        }
        if (DEBUG) printf("Linking %p to %p\n", freed, curr);
        freed->next = curr;
        assert(freed < curr);
        if (DEBUG) {
          printf("Free list after freeing\n");
          print_free_list();
        }
        coalesce();
        attempt_to_free_pages();
        if (DEBUG && entry) {
          if (DEBUG) printf("Free list after free\n");
          print_free_list();
        }
        return;
      }
    }
    prev = curr;
    curr = curr->next;
  }
}

void
coalesce()
{
  header_t *curr = get_head();
  while (curr->next != NULL)
  {
    if ((char*)curr + curr->size + sizeof(header_t) == curr->next &&
        BASEADDR(curr) == BASEADDR(curr->next)) {
      if (DEBUG) printf("Coalescing %p and %p\n", curr, curr->next);
      assert((void*)curr < (void*)curr->next);
      curr->size += ((header_t*)(curr->next))->size + sizeof(header_t);
      curr->next =  ((header_t*)(curr->next))->next;
      assert(curr->size <= (PAGESIZE - sizeof(header_t) - sizeof(void*)));
      if (DEBUG) {
        printf("Free list after coalesce\n");
        print_free_list();
      }

      if (BASEADDR(curr) == BASEADDR(curr->next)) assert((void*)curr < (void*)curr->next || curr->next == NULL);
    } else {
      curr = curr->next;
    }
  }
}

void
attempt_to_free_pages()
{
  if (get_head() == NULL) {
    if (DEBUG) printf("Freeing storage page\n");
    free_page(entry);
    entry = NULL;
    return;
  }
  header_t *curr, *prev;
  kma_page_t *page;
  prev = NULL;
  curr = get_head();
  while (curr != NULL) {
    //printf("%p has %d bytes available\n", BASEADDR(curr), curr->size);
    if (curr->size == PAGESIZE - sizeof(header_t) - sizeof(void*)) {
      if (DEBUG) printf("%p is empty, attemping to free it.\n", curr);
      if (curr == get_head()) {
        page = *((kma_page_t**)(BASEADDR(curr)));
        move_head((header_t**)&(curr->next));
        if (DEBUG) printf("Freeing page that was the start of the list, %p\n", curr);
        free_page(page);
        attempt_to_free_pages();
        return;
      } else {
        if (DEBUG) printf("Freeing the a page from the list\n");
        assert(prev != NULL);
        page = *((kma_page_t**)(BASEADDR(curr)));
        prev->next = curr->next;
        free_page(page);
        attempt_to_free_pages();
        return;
      }
    }
    prev = curr;
    curr = curr->next;
  }
}

void
print_free_list()
{
  header_t *curr = get_head();
  while (curr != NULL) {
    printf("PAGE: %p, %d - <%d, %p>\n", BASEADDR(curr),(int)((void*)curr - BASEADDR(curr)), curr->size, curr->next);
    curr = curr->next;
  }
}

void
check_list()
{
  header_t *curr = get_head();
  while (curr != NULL) {
    if (((void*)curr > (void*)curr->next) && curr->next != NULL &&
        BASEADDR(curr) == BASEADDR(curr->next)) {
      printf("Linked list is out of order! %p is linked to %p\n", curr, curr->next);
      //print_free_list();
      assert((void*)curr < (void*)curr->next);
    }
    curr = curr->next;
  }
}

void
init_page(kma_page_t **page)
{
  if (DEBUG) printf("Initializing page\n");

  if (DEBUG) printf("Copying pointer to kma_page to start of page\n");
  memcpy((*page)->ptr, page, sizeof(void*));

  header_t header = {
    .size = (PAGESIZE - sizeof(header_t) - sizeof(void*)),
    .next = NULL
  };

  if (DEBUG) printf("Copying first header\n");
  memcpy((char*)((*page)->ptr) + sizeof(void*), &header, sizeof(header_t));
}

#endif // KMA_RM
