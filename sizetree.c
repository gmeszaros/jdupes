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


void sizetree_add(file_t *file)
{
  struct sizetree *cur;

  if (file == NULL) jc_nullptr("sizetree_add");
  LOUD(fprintf(stderr, "sizetree_add('%s')\n", file->d_name);)

  if (sizetree_head == NULL) {
    sizetree_head = sizetree_alloc(file);
    return;
  }

  cur = sizetree_head;
  while (1) {
    if (file->size == cur->size) {
      file->next = cur->list;
      cur->list = file;
      return;
    }

    if (file->size < cur->size) {
      if (cur->left == NULL) {
        cur->left = sizetree_alloc(file);
	return;
      }
      cur = cur->left;
      continue;
    }

    if (file->size > cur->size) {
      if (cur->right == NULL) {
        cur->right = sizetree_alloc(file);
	return;
      }
      cur = cur->right;
      continue;
    }
  }
}
