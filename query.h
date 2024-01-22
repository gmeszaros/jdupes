/* Query match data
 * This file is part of jdupes; see jdupes.c for license information */

#ifndef JDUPES_QUERY_H
#define JDUPES_QUERY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _querystate {
	struct _querystate *next;
	int count;
	int alloc;
	file_t *list[];
} qstate_t;

#define QS_NONE       0x00
#define QS_NAME       0x01
#define QS_SIZE       0x02
#define QS_MTIME      0x03
#define QS_CTIME      0x04
#define QS_ATIME      0x05
#define QS_PARAMORDER 0x06
#define QS_DEVICE     0x07
#define QS_INODE      0x08
#define QS_LINKS      0x09

#define QS_NOFLAGS    0x0f
#define QS_FLAGS      0xf0
#define QS_INVERTED   0x80

void qstate_sort_sets(qstate_t **qstate_ptr, int sort_type);
void qstate_sort_lists(qstate_t *qstate, int sort_type);
qstate_t *query_new_state(void);

#ifdef __cplusplus
}
#endif

#endif /* JDUPES_QUERY_H */
