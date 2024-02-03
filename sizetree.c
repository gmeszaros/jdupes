/* jdupes file size binary search tree
 * This file is part of jdupes; see jdupes.c for license information */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libjodycode.h>

#include "jdupes.h"
#include "likely_unlikely.h"
#include "sizetree.h"


/* Number of slots to add at a time; must be at least 2 */
#define SIZETREE_ALLOC_SLOTS 2

static struct sizetree *sizetree_head = NULL;


/* Allocate a sizetree node */
static struct sizetree *sizetree_alloc(file_t *file)
{
	struct sizetree *node;

	node = (struct sizetree *)calloc(1, sizeof(struct sizetree));
	if (node == NULL) jc_oom("sizetree_alloc");
	node->size = file->size;
	node->list = file;
	return node;
}


/* Allocate a sizetree iteration state */
st_state_t *sizetree_new_state(void)
{
	st_state_t *st = (st_state_t *)malloc(sizeof(st_state_t));
	if (st == NULL) jc_oom("sizetree_new_state");
	memset(st, 0, sizeof(st_state_t));
	st->stackcnt = -1;
	return st;
}


void sizetree_free_state(st_state_t *st)
{
	if (st->stack != NULL) free(st->stack);
	free(st);
	return;
}


/* Return the next file list; stackcnt == -1 resets/inits the state */
file_t *sizetree_next_list(st_state_t *st)
{
	struct sizetree *cur;

	if (st->stackcnt == -1 || st->stack == NULL) {
		/* Initialize everything and push head of tree as first stack item */
		if (st->stack != NULL) free(st->stack);
		st->stack = (struct sizetree **)malloc(sizeof(struct sizetree *) * SIZETREE_ALLOC_SLOTS);
		st->stackslots = SIZETREE_ALLOC_SLOTS;
		st->stackcnt = 1;
		st->stack[0] = sizetree_head;
	}
	/* Pop one off the stack */
	if (st->stackcnt < 1) return NULL;
	cur = st->stack[--st->stackcnt];
	if (cur == NULL) return NULL;

	if (st->stackslots - st->stackcnt < 2) {
		void *tempalloc = realloc(st->stack, sizeof(struct sizetree *) * (SIZETREE_ALLOC_SLOTS + st->stackslots));
		if (tempalloc == NULL) jc_oom("sizetree_alloc realloc");
		st->stack = (struct sizetree **)tempalloc;
		st->stackslots += SIZETREE_ALLOC_SLOTS;
	}

	/* Push left/right nodes to stack and return current node's list */
	if (cur->right != NULL) st->stack[st->stackcnt++] = cur->right;
	if (cur->left != NULL) st->stack[st->stackcnt++] = cur->left;
	return cur->list;
}


/* Add a file to the size tree */
void sizetree_add(file_t *file)
{
	struct sizetree *cur;

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
