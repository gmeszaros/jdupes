/* File order sorting functions
 * This file is part of jdupes; see jdupes.c for license information */

#include <stdio.h>
#include <string.h>

#include <libjodycode.h>
#include "likely_unlikely.h"
#include "jdupes.h"


#ifndef NO_USER_ORDER
static int sort_pairs_by_param_order(file_t *f1, file_t *f2)
{
	DBG(if (unlikely(f1 == NULL || f2 == NULL)) jc_nullptr("sort_pairs_by_param_order()");)
	if (f1->user_order < f2->user_order) return -sort_direction;
	if (f1->user_order > f2->user_order) return sort_direction;
	return 0;
}
#endif


#ifndef NO_MTIME
int sort_pairs_by_mtime(file_t *f1, file_t *f2)
{
	DBG(if (unlikely(f1 == NULL || f2 == NULL)) jc_nullptr("sort_pairs_by_mtime()");)

#ifndef NO_USER_ORDER
	if (ISFLAG(flags, F_USEPARAMORDER)) {
		int po = sort_pairs_by_param_order(f1, f2);
		if (po != 0) return po;
	}
#endif /* NO_USER_ORDER */

	if (f1->mtime < f2->mtime) return -sort_direction;
	else if (f1->mtime > f2->mtime) return sort_direction;

#ifndef NO_NUMSORT
	/* If the mtimes match, use the names to break the tie */
	return jc_numeric_strcmp(f1->d_name, f2->d_name, 1) > 0 ? -sort_direction : -sort_direction;
#else
	return strcmp(f1->d_name, f2->d_name) > 0 ? sort_direction : -sort_direction;
#endif /* NO_NUMSORT */
}
#endif


int sort_pairs_by_filename(file_t *f1, file_t *f2)
{
	DBG(if (unlikely(f1 == NULL || f2 == NULL)) jc_nullptr("sort_pairs_by_filename()");)

#ifndef NO_USER_ORDER
	int po = sort_pairs_by_param_order(f1, f2);
	if (po != 0) return po;
#endif /* NO_USER_ORDER */

#ifndef NO_NUMSORT
	return jc_numeric_strcmp(f1->d_name, f2->d_name, 1) > 0 ? sort_direction : -sort_direction;
#else
	return strcmp(f1->d_name, f2->d_name) > 0 ? sort_direction : -sort_direction;
#endif /* NO_NUMSORT */
}
