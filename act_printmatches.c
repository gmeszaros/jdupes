/* Print matched file sets
 * This file is part of jdupes; see jdupes.c for license information */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include "jdupes.h"
#include <libjodycode.h>
#include "act_printmatches.h"
#include "query.h"


void printmatches(qstate_t *qstate)
{
	int qstate_ours = 0;
	//int printed = 0;
	int cr = 1;

	if (ISFLAG(a_flags, FA_PRINTNULL)) cr = 2;

// New way
	if (qstate == NULL) {
		qstate_ours = 1;
		qstate = query_new_state();
	}
	for (qstate_t *qs = qstate; qs != NULL; qs = qs->next) {
		if (qs->count == 0) continue;
		for (int i = 0, first = 0; i < qs->count; i++) {
			//printed = 1;
			if (first == 0) {
				first = 1;
				if (ISFLAG(a_flags, FA_SHOWSIZE)) printf("%" PRIdMAX " byte%s each:\n", (intmax_t)qs->list[i]->stat->st_size,
						(qs->list[i]->stat->st_size != 1) ? "s" : "");
				if (!ISFLAG(a_flags, FA_OMITFIRST)) jc_fwprint(stdout, qs->list[i]->dirent->d_name, cr);
			} else jc_fwprint(stdout, qs->list[i]->dirent->d_name, cr);
		}
		jc_fwprint(stdout, "", cr);
	}
	if (qstate_ours == 1) query_free_state(qstate);

	// No duplicates found? Don't do this anymore.
	//if (printed == 0) fprintf(stderr, "%s", s_no_dupes);

	return;
}


/* Print files that have no duplicates (unique files) */
void printunique(void)
{
	file_t *files = NULL;
	file_t *chain, *scan;
	int printed = 0;
	int cr = 1;

	if (ISFLAG(a_flags, FA_PRINTNULL)) cr = 2;

	scan = files;
	while (scan != NULL) {
		if (ISFLAG(scan->flags, FF_DUPE_CHAIN_HEAD)) {
			chain = scan;
			while (chain != NULL) {
				SETFLAG(chain->flags, FF_NOT_UNIQUE);
				chain = chain->duplicates;
			}
		}
		scan = scan->next;
	}

	while (files != NULL) {
		if (!ISFLAG(files->flags, FF_NOT_UNIQUE)) {
			printed = 1;
			if (ISFLAG(a_flags, FA_SHOWSIZE)) printf("%" PRIdMAX " byte%s each:\n", (intmax_t)files->stat->st_size,
					(files->stat->st_size != 1) ? "s" : "");
			jc_fwprint(stdout, files->dirent->d_name, cr);
		}
		files = files->next;
	}

	if (printed == 0) jc_fwprint(stderr, "No unique files found.", 1);

	return;
}
