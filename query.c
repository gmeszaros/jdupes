/* Query match data
 * This file is part of jdupes; see jdupes.c for license information */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <libjodycode.h>
#include "jdupes.h"
#include "sizetree.h"
#include "query.h"

#define QSALLOC_CHUNK_SIZE 4

static void qstate_add_to_list(qstate_t **qstate, file_t *cur);
static void qstate_file_swap(file_t **file1, file_t **file2);


static void qstate_file_swap(file_t **file1, file_t **file2)
{
	if (jc_numeric_strcmp((*file1)->d_name, (*file2)->d_name, 0) > 0) {
		file_t *temp = *file1;
		*file1 = *file2;
		*file2 = temp;
	}
	return;
}


/* Sort the match sets by their list heads */
void qstate_sort_sets(qstate_t **qstate_ptr, int sort_type)
{
	int sort, done;

	if (qstate_ptr == NULL || *qstate_ptr == NULL) return;

	sort = sort_type & QS_NOFLAGS;
	if (sort == QS_NONE) return;

	if (sort == QS_NAME) {
		while (1) {
			done = 1;
			qstate_t *prev = NULL;
			qstate_t *cur = *qstate_ptr;
			for (; cur != NULL && cur->next != NULL; prev = cur, cur = cur->next) {
				if (jc_numeric_strcmp(cur->list[0]->d_name, cur->next->list[0]->d_name, 0) > 0) {
//fprintf(stderr, "swap: '%s', '%s'\n", cur->list[0]->d_name, cur->next->list[0]->d_name);
					/* Swap the list items */
					qstate_t *temp = cur->next;
					if (prev == NULL) *qstate_ptr = temp;
					else prev->next = temp;
					cur->next = temp->next;
					temp->next = cur;
					done = 0;
				}
			}
			if (done == 1) break;
		}
	}

	return;
}


/* Sort each match list */
void qstate_sort_lists(qstate_t *qs, int sort_type)
{
	int sort, done;

	if (qs == NULL) return;

	sort = sort_type & QS_NOFLAGS;
	if (sort == QS_NONE) return;

	if (sort == QS_NAME) {
		for (; qs != NULL; qs = qs->next) {
			if (qs->count < 2) continue;

			while (1) {
				done = 1;
				for (int i = 0; (i + 1) < qs->count; i++) {
					for (int j = i + 1; j < qs->count; j++) {
//int c = jc_numeric_strcmp(qs->list[i]->d_name, qs->list[j]->d_name, 0);
//if (c == 0) c = '='; else if (c > 0) c = '>'; else c = '<';
//fprintf(stderr, "qstate_sort[%d,%d]: '%s' %c '%s'\n", i, j, qs->list[i]->d_name, c, qs->list[j]->d_name);
						if (jc_numeric_strcmp(qs->list[i]->d_name, qs->list[j]->d_name, 0) > 0) {
							qstate_file_swap(&(qs->list[i]), &(qs->list[j]));
							done = 0;
						}
					}
				}
				if (done == 1) break;
			}
		}
	}

	return;
}


static void qstate_add_to_list(qstate_t **qstate, file_t *cur)
{
	const char func_name[] = "qstate_add_to_list";
	qstate_t *qs = *qstate;

	if (qstate == NULL || *qstate == NULL) jc_nullptr(func_name);

	if (qs->count == qs->alloc) {
		qs->alloc += QSALLOC_CHUNK_SIZE;
		qs = (qstate_t *)realloc(qs, sizeof(qstate_t) + sizeof(file_t *) * qs->alloc);
		if (qs->list == NULL) jc_oom(func_name);
	}

	qs->list[qs->count] = cur;
//fprintf(stderr, "qstate_add_to_list [%p, %d]: %s\n", qs, qs->count, qs->list[qs->count]->d_name);
	qs->count++;

	*qstate = qs;
	return;
}


qstate_t *query_new_state(void)
{
	st_state_t *st_state;
	qstate_t *qs = NULL, *prev = NULL, *head = NULL;

	st_state = sizetree_state_alloc();
	if (st_state == NULL) jc_oom("query_new_state st_state");

	st_state->stackcnt = -1;
	for (file_t *st_next = sizetree_next_list(st_state); st_next != NULL; st_next = sizetree_next_list(st_state)) {
		for (; st_next != NULL; st_next = st_next->next) {
			/* Get each duplicate set and put into a query state list */
			if (ISFLAG(st_next->flags, FF_DUPE_CHAIN_HEAD)) {
				qs = (qstate_t *)calloc(1, sizeof(qstate_t) + (sizeof(file_t *) * QSALLOC_CHUNK_SIZE));
				if (qs == NULL) jc_oom("query_new_state");
				qs->alloc = QSALLOC_CHUNK_SIZE;
				for (file_t *cur = st_next; cur != NULL; cur = cur->duplicates) qstate_add_to_list(&qs, cur);
				if (head == NULL) head = qs;
				else if (prev != NULL) prev->next = qs;
				prev = qs;
			}
		}
	}

	return head;
}
