/* jdupes file size binary search tree
 * This file is part of jdupes; see jdupes.c for license information */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libjodycode.h>

#include "jdupes.h"
#include "likely_unlikely.h"


#define SIZETREE_ALLOC_SLOTS 2

struct sizetree {
  struct sizetree *left;
  struct sizetree *right;
  file_t *list;
  off_t size;
};

static struct sizetree *sizetree_head = NULL;


static struct sizetree *sizetree_alloc(file_t *file)
{
  struct sizetree *node;

  if (file == NULL) jc_nullptr("sizetree_alloc");
  LOUD(fprintf(stderr, "sizetree_alloc('%s' [%ld])\n", file->d_name, file->size);)
  node = (struct sizetree *)calloc(1, sizeof(struct sizetree));
  if (node == NULL) jc_oom("sizetree_alloc");
  node->size = file->size;
  node->list = file;
  return node;
}


/* Return the next file list; reset: 1 = restart, -1 = free resources */
file_t *sizetree_next_list(int reset)
{
  struct sizetree *cur;
  static struct sizetree **st_stack = NULL;
  static int stackcnt, stackslots;

  LOUD(fprintf(stderr, "sizetree_next_list(%d)\n", reset);)

  if (unlikely(reset == -1)) {
    if (st_stack != NULL) free(st_stack);
    return NULL;
  }

  if (reset == 1 || st_stack == NULL) {
    /* Initialize everything and push head of tree */
    if (st_stack != NULL) free(st_stack);
    stackcnt = 1;
    st_stack = (struct sizetree **)malloc(sizeof(struct sizetree *) * SIZETREE_ALLOC_SLOTS);
    stackslots = SIZETREE_ALLOC_SLOTS;
    st_stack[0] = sizetree_head;
fprintf(stderr, "st: stack: pushed X %p\n", sizetree_head);
    return NULL;
  } else {
    /* Pop one off the stack */
    if (stackcnt < 1) return NULL;
    cur = st_stack[--stackcnt];
fprintf(stderr, "st: stack: popped %p [%ld]\n", cur, cur->size);
    if (cur == NULL) return NULL;
  }

  if (stackslots - stackcnt < 2) {
    st_stack = (struct sizetree **)realloc(st_stack, stackslots + sizeof(struct sizetree *) * SIZETREE_ALLOC_SLOTS);
    stackslots += SIZETREE_ALLOC_SLOTS;
  }

  /* Push left/right nodes to stack and return current node's list */
  if (cur->right != NULL) st_stack[stackcnt++] = cur->right;
if (cur->right != NULL) fprintf(stderr, "st: stack: pushed R %p\n", st_stack[stackcnt - 1]);
  if (cur->left != NULL) st_stack[stackcnt++] = cur->left;
if (cur->left != NULL) fprintf(stderr, "st: stack: pushed L %p\n", st_stack[stackcnt - 1]);
  return cur->list;
}


void sizetree_add(file_t *file)
{
  struct sizetree *cur;

  if (file == NULL) jc_nullptr("sizetree_add");
  LOUD(fprintf(stderr, "sizetree_add([%p] '%s')\n", file, file->d_name);)

  if (sizetree_head == NULL) {
    LOUD(fprintf(stderr, "sizetree_add: new sizetree_head size %ld\n", file->size);)
    sizetree_head = sizetree_alloc(file);
    return;
  }

  cur = sizetree_head;
  while (1) {
    if (file->size == cur->size) {
      LOUD(fprintf(stderr, "sizetree_add: attaching to list [%p] size %ld\n", cur, cur->size);)
      file->next = cur->list;
      cur->list = file;
      return;
    }

    if (file->size < cur->size) {
      if (cur->left == NULL) {
        cur->left = sizetree_alloc(file);
        LOUD(fprintf(stderr, "sizetree_add: creating new list L [%p] size %ld\n", cur->left, file->size);)
	return;
      }
      LOUD(fprintf(stderr, "sizetree_add: going left (%ld < %ld)\n", file->size, cur->size);)
      cur = cur->left;
      continue;
    }

    if (file->size > cur->size) {
      if (cur->right == NULL) {
        cur->right = sizetree_alloc(file);
        LOUD(fprintf(stderr, "sizetree_add: creating new list R [%p] size %ld\n", cur->right, file->size);)
	return;
      }
      LOUD(fprintf(stderr, "sizetree_add: going right (%ld > %ld)\n", file->size, cur->size);)
      cur = cur->right;
      continue;
    }
  }
}
