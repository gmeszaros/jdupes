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
  struct sizetree *node = (struct sizetree *)calloc(1, sizeof(struct sizetree));
  if (node == NULL) jc_oom("sizetree_alloc");
  node->size = file->size;
  node->list = file;
  return node;
}


/* Return the next file list; reset: 1 = restart, -1 = free resources */
file_t *sizetree_next_list(int reset)
{
  struct sizetree *cur;
  static struct sizetree **stack = NULL;
  static int stackcnt, stackslots;

  LOUD(fprintf(stderr, "sizetree_next_list(%d)\n", reset);)

  if (unlikely(reset == -1)) {
    if (stack != NULL) free(stack);
    return NULL;
  }
  if (reset == 1 || stack == NULL) {
    /* Initialize everything and push head of tree */
    if (stack != NULL) free(stack);
    stackcnt = 1;
    stack = (struct sizetree **)realloc(stack, stackslots + sizeof(struct sizetree *) * SIZETREE_ALLOC_SLOTS);
    stackslots = SIZETREE_ALLOC_SLOTS;
    if (sizetree_head == NULL) return NULL;
    stack[0] = sizetree_head;
    return NULL;
  } else {
    /* Pop one off the stack */
    if (stackcnt < 1) return NULL;
    cur = stack[--stackcnt];
  }

  if (stackslots - stackcnt < 2) {
    stack = (struct sizetree **)realloc(stack, stackslots + sizeof(struct sizetree *) * SIZETREE_ALLOC_SLOTS);
    stackslots += SIZETREE_ALLOC_SLOTS;
  }

  /* Push left/right nodes to stack and return current node's list */
  if (cur->left != NULL) stack[stackcnt++] = cur->left;
  if (cur->right != NULL) stack[stackcnt++] = cur->right;
  return cur->list;
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
