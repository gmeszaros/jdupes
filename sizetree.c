/* jdupes file size binary search tree
 * This file is part of jdupes; see jdupes.c for license information */

#ifdef __linux__
 #include <fcntl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libjodycode.h>

#include "jdupes.h"
#include "likely_unlikely.h"


#define SIZETREE_ALLOC_SLOTS 128

struct sizetree {
  struct sizetree *left;
  struct sizetree *right;
  file_t *list;
  off_t size;
};

static struct sizetree *sizetree_head = NULL;
static file_t **scanlist = NULL;
static unsigned int scancnt, scanslots;


static struct sizetree *sizetree_alloc(file_t *file)
{
  struct sizetree *node = (struct sizetree *)calloc(1, sizeof(struct sizetree));
  if (node == NULL) jc_oom("sizetree_alloc");
  node->size = file->size;
  node->list = file;
  return node;
}


void sizetree_list_free(void)
{
  if (scanlist != NULL) {
    free(scanlist);
    scanlist = NULL;
  }
  scancnt = 0;
  scanslots = 0;
  return;
}


/* Return the next file list. Call with *id=0 to start a fresh scan */
file_t *sizetree_next_list(unsigned int *id)
{
  struct sizetree *cur;

  if (*id == 0) {
    sizetree_list_free();
    cur = sizetree_head;
    if (cur == NULL) return NULL;
    while (1) {
      // allocate scan result array in chunks
      if (scancnt == scanslots) {
        scanlist = (file_t **)realloc(scanlist, scanslots + sizeof(file_t *) * SIZETREE_ALLOC_SLOTS);
        scanslots += SIZETREE_ALLOC_SLOTS;
      }
      // TODO: recurse the tree, adding lists as we go
    }
    return scanlist[0];
  }
  if (*id > scancnt) return NULL;
  (*id)++;
  return scanlist[*id];
}


void sizetree_add(file_t *file)
{
  struct sizetree *cur;

  if (file == NULL) jc_nullptr("sizetree_add");
  LOUD(fprintf(stderr, "sizetree_add('%s')\n", file->d_name);)

  if (sizetree_head == NULL) {
    LOUD(fprintf(stderr, "sizetree_add: new sizetree_head size %ld\n", file->size);)
    sizetree_head = sizetree_alloc(file);
    return;
  }

  cur = sizetree_head;
  while (1) {
    if (file->size == cur->size) {
      LOUD(fprintf(stderr, "sizetree_add: attaching to list size %ld\n", cur->size);)
      file->next = cur->list;
      cur->list = file;
      return;
    }

    if (file->size < cur->size) {
      if (cur->left == NULL) {
        LOUD(fprintf(stderr, "sizetree_add: creating new list size %ld\n", file->size);)
        cur->left = sizetree_alloc(file);
	return;
      }
      LOUD(fprintf(stderr, "sizetree_add: going left (%ld < %ld)\n", file->size, cur->size);)
      cur = cur->left;
      continue;
    }

    if (file->size > cur->size) {
      if (cur->right == NULL) {
        LOUD(fprintf(stderr, "sizetree_add: creating new list size %ld\n", file->size);)
        cur->right = sizetree_alloc(file);
	return;
      }
      LOUD(fprintf(stderr, "sizetree_add: going right (%ld > %ld)\n", file->size, cur->size);)
      cur = cur->right;
      continue;
    }
  }
}
