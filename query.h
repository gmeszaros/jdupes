/* Query match data
 * This file is part of jdupes; see jdupes.c for license information */

#ifndef JDUPES_QUERY_H
#define JDUPES_QUERY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _querystate {
	struct _querystate *next;
	struct _querychain *chain;
} qstate_t;

typedef struct _querychain {
	struct _querychain *next;
	file_t *head[];
} qchain_t;

#define QF_SORT_NAME       (1U << 0)
#define QF_SORT_SIZE       (1U << 1)
#define QF_SORT_MTIME      (1U << 2)
#define QF_SORT_CTIME      (1U << 3)
#define QF_SORT_ATIME      (1U << 4)
#define QF_SORT_PARAMORDER (1U << 5)
#define QF_SORT_DEVICE     (1U << 6)
#define QF_SORT_INODE      (1U << 7)
#define QF_SORT_LINKS      (1U << 8)

qstate_t *query_new_state(uint32_t options);

#ifdef __cplusplus
}
#endif

#endif /* JDUPES_QUERY_H */
