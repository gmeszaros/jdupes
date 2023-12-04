/* jdupes file size binary search tree
 * This file is part of jdupes; see jdupes.c for license information */

#ifndef JDUPES_SIZETREE_H
#define JDUPES_SIZETREE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "jdupes.h"

struct sizetree {
	off_t size;
	struct sizetree *left;
	struct sizetree *right;
	file_t *list;
};

typedef struct _sizetree_state {
	struct sizetree **stack;
	int stackcnt;
	int stackslots;
} st_state_t;

st_state_t *sizetree_state_alloc(void);
void sizetree_add(file_t *file);
file_t *sizetree_next_list(st_state_t *st);

#ifdef __cplusplus
}
#endif

#endif /* JDUPES_SIZETREE_H */
