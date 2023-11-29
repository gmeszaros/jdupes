/* jdupes file size binary search tree
 * This file is part of jdupes; see jdupes.c for license information */

#ifndef JDUPES_SIZETREE_H
#define JDUPES_SIZETREE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "jdupes.h"

struct sizetree {
  struct sizetree *left;
  struct sizetree *right;
  file_t *list;
  off_t size;
};

struct sizetree_state {
  struct sizetree **stack;
  int stackcnt;
  int stackslots;
  int reset;
};

void sizetree_add(file_t *file);
file_t *sizetree_next_list(struct sizetree_state *st);

#ifdef __cplusplus
}
#endif

#endif /* JDUPES_SIZETREE_H */
