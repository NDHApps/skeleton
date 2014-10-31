/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the buddy algorithm
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
#ifdef KMA_BUD
#define __KMA_IMPL__
#define DEBUG 0

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
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

typedef struct
{
    int allocs;
    int bufsizes[10];
    void* lists[10];
    
} freelist_t;

typedef struct
{
    kma_page_t* page;
    void* next;
    char bitmap[128];
} page_t;

/************Global Variables*********************************************/
static kma_page_t* g_page = NULL;
/************Function Prototypes******************************************/
void init_page();
void add_to_free_list(void*, int);
void* get_free_block(kma_size_t);
void alloc_page();
void free_kma_pages();
void update_bitmap(void*,kma_size_t,int);
int coalesce(void*,int);

/************External Declaration*****************************************/

/**************Implementation***********************************************/

void*
kma_malloc(kma_size_t size)
{
    // init global pointer if not already initiliazed
    if (!g_page) {
        init_page();
    }
    
    // add enough space for a pointer to the size
    size += sizeof(int);
    void* address;
    
    // ensure that there is enough space
    if (size + sizeof(page_t) + sizeof(kma_page_t) + sizeof(freelist_t) > PAGESIZE) {
        
        //allocate a new page
        kma_page_t* page = get_page();
        *((kma_page_t**)page->ptr) = page;
        if ((size + sizeof(kma_page_t*)) > page->size)
        {
            //
            free_page(page);
            return NULL;
        }
        return page->ptr + sizeof(kma_page_t*);
    }
    
    address = get_free_block(size);
    
    if (address != NULL) {
        update_bitmap(address-sizeof(int), *((int*)(address-sizeof(int))), 1);
        return address;
    }
    
    //
    alloc_page();
    
    address = get_free_block(size);
    if (address != NULL) {
        update_bitmap(address-sizeof(int), *((int*)(address-sizeof(int))), 1);
        return address;
    }
    return NULL;
}

void
kma_free(void* ptr, kma_size_t size)
{
    freelist_t* list = (freelist_t *)(g_page->ptr + sizeof(page_t));
    if (size > PAGESIZE-sizeof(page_t)-sizeof(kma_page_t)-sizeof(freelist_t)-sizeof(int)) {
        kma_page_t* page = *((kma_page_t**)(ptr - sizeof(kma_page_t*)));
        free_page(page);
        return;
    }
    
    ptr = (ptr - sizeof(int));
    int mysize = *((int *) ptr);
    
    update_bitmap(ptr, mysize, 0);
    mysize = coalesce(ptr,mysize);
    
    add_to_free_list(ptr, mysize);
    
    list->allocs--;
    if (list->allocs <= 0)
        free_kma_pages();
}

void
free_kma_pages()
{
    page_t* p = g_page->ptr;
    page_t* next_p;
    while (p != NULL) {
        next_p = p->next;
        (p->page)->ptr = (void*)p;
        free_page(p->page);
        p = next_p;
    }
    g_page = NULL;
}

void*
get_free_block(kma_size_t size)
{
    freelist_t* list = (freelist_t*)(g_page->ptr + sizeof(page_t));
    if (size > list->bufsizes[9]) {
        return NULL;
    }
    int i=0;
    while (list->bufsizes[i] < size) {
        i++;
    }
    int idx = i;
    while (list->lists[i] == NULL) {
        i++;
        if (i == 10) {
            return NULL;
        }
    }
    while (i > idx) {
        void* address = list->lists[i];
        void* nextaddr = *((void**)address);
        list->lists[i] = nextaddr;
        
        nextaddr = list->lists[i-1];
        list->lists[i-1] = address;
        if (i == 9) { // special case, because bufsizes[9] != bufsizes[8]*2
            *((void**)address) = nextaddr;
        } else {
            *((void**)address) = address + list->bufsizes[i-1];
            address = *((void**)address);
            *((void**)address) = nextaddr;
        }
        
        i--;
    }
    void* returnaddr = list->lists[idx];
    list->lists[idx] = *((void**)returnaddr);
    *((int*)returnaddr) = list->bufsizes[idx];
    list->allocs++;
    return returnaddr+sizeof(int);
}

void init_page()
{
    if (DEBUG)
        printf("Initializing page\n");
    int space = (unsigned int)PAGESIZE - sizeof(kma_page_t) - sizeof(page_t) - sizeof(freelist_t);
    kma_page_t* new_kma_page = get_page();
    page_t* new_page = (page_t *)(new_kma_page->ptr);
    
    new_page->page = new_kma_page;
    new_page->next = NULL;
    g_page = new_kma_page;
    
    freelist_t* list = (freelist_t*)((void *)new_page + sizeof(page_t));
    list->allocs = 0;
    
    int i;
    int size = 16;
    for(i = 0; i < 10; i++) {
        list->bufsizes[i] = size;
        list->lists[i] = NULL;
        size *= 2;
    }
    for (i = 0; i < 128; i++) {
        new_page->bitmap[i] = 0;
    }
    list->bufsizes[9] = space;
    void* nextaddr = (void*)new_page + sizeof(page_t) + sizeof(freelist_t);
    add_to_free_list(nextaddr,list->bufsizes[9]);
}

void alloc_page()
{
    int space = (unsigned int)PAGESIZE - sizeof(kma_page_t) - sizeof(page_t) - sizeof(freelist_t);
    
    kma_page_t* new_kma_page = get_page();
    page_t* new_page = (page_t *)(new_kma_page->ptr);
    new_kma_page->ptr = (void*)new_page;
    
    new_page->page = new_kma_page;
    new_page->next = NULL;
    int i;
    for (i = 0; i < 128; i++) {
        new_page->bitmap[i] = 0;
    }
    
    page_t* old_page = (page_t*)(g_page->ptr);
    while (old_page->next != NULL) {
        old_page = old_page->next;
    }
    old_page->next = new_page;
    
    // don't really need to add sizeof(freelist_t), but max. buffer size will be bufsizes[9] regardless so might as well do it for consistency
    void* startAddr = (void*)(new_page) + sizeof(page_t) + sizeof(freelist_t);
    add_to_free_list(startAddr,space);
}

void add_to_free_list (void* addr, int size) // size INCLUDES head ptr
{
    freelist_t* list = (freelist_t*)(g_page->ptr + sizeof(page_t));
    int i;
    for (i = 0; i < 10; i ++) {
        if (size == list->bufsizes[i]) {
            *((void **)addr) = list->lists[i];
            list->lists[i] = addr;
            break;
        }
    }
}


void update_bitmap(void* ptr, kma_size_t size, int mem_status) {
    
    page_t* page = (page_t*)(g_page->ptr);
    while (ptr < (void*)page || ptr > (void*)page+PAGESIZE-sizeof(kma_page_t)) {
        page = (page_t*)(page->next);
    }
    int offset = (ptr - (void*)page) - sizeof(page_t) - sizeof(freelist_t);
    int i;
    if (mem_status == 1) {
        for (i = offset/16; i < offset/16 + size/16; i++) {
            page->bitmap[i/8] |= (1 << (7 - (i%8)));
        }
    }
    else {
        for (i = offset/16; i < offset/16 + size/16; i++) {
            page->bitmap[i/8] &= ~(1 << (7 - (i%8)));
        }
    }
}


int coalesce(void* ptr, int size) {
    
    freelist_t* list = (freelist_t*)(g_page->ptr + sizeof(page_t));
    if (2*size > list->bufsizes[9]) {
        return size;
    }
    
    page_t* page = (page_t*)(g_page->ptr);
    while (ptr < ((void*)page+sizeof(page_t)+sizeof(freelist_t)) || ptr > (void*)page+PAGESIZE-sizeof(kma_page_t)) {
        page = (page_t*)(page->next);
    }
    int offset = (ptr - (void*)page) - sizeof(page_t) - sizeof(freelist_t);
    
    void* oldptr;
    int startbit;
    
    if ((offset/size) % 2 == 0) {
        startbit = offset/16 + size/16;
        oldptr = ptr + size;
    } else {
        startbit = offset/16 - size/16;
        oldptr = ptr - size;
    }
    int i;
    
    for (i=0; i < size/16; i++) {
        
        int j = 0;
        int k = startbit+i;
        while (k >= sizeof(char)) {
            j++;
            k -= sizeof(char);
        }
        if (page->bitmap[j] & (1 << (7 - k)))
            return size;
    }
    
    for (i=0; list->bufsizes[i] != size; i++) {}
    void* curptr = list->lists[i];
    
    while (curptr != NULL && curptr > ((void*)page+sizeof(page_t)+sizeof(freelist_t)) && curptr < (void*)page+PAGESIZE-sizeof(kma_page_t)) {
        
        if (*((void**)curptr) == oldptr) {
            *((void**)curptr) = *((void**)oldptr);
            
            if (oldptr < ptr) {
                ptr = oldptr;
            }
            return 2*size;
        }
        curptr = *((void**)curptr);
    }
    return size;
}

#endif // KMA_BUD