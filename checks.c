/* jdupes file check functions
 * This file is part of jdupes; see jdupes.c for license information */

#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <inttypes.h>

#include <libjodycode.h>
#include "likely_unlikely.h"
#ifndef NO_EXTFILTER
 #include "extfilter.h"
#endif
#include "filestat.h"
#include "jdupes.h"


/***** End definitions, begin code *****/

/***** Add new functions here *****/


/* Check a pair of files for match exclusion conditions
 * Returns:
 *  0 if all condition checks pass
 * -1 or 1 on compare result less/more
 * -2 on an absolute exclusion condition met
 *  2 on an absolute match condition met
 * -3 on exclusion due to isolation
 * -4 on exclusion due to same filesystem
 * -5 on exclusion due to permissions */
int check_conditions(const file_t * const restrict file1, const file_t * const restrict file2)
{
	DBG(if (unlikely(file1 == NULL || file2 == NULL || file1->d_name == NULL || file2->d_name == NULL)) jc_nullptr("check_conditions()");)

	/* Do not compare files that already have existing duplicates */
	if (ISFLAG(file2->flags, FF_HAS_DUPES)) return -6;

#ifndef NO_USER_ORDER
	/* Exclude based on -I/--isolate */
	if (ISFLAG(flags, F_ISOLATE) && (file1->user_order == file2->user_order)) return -3;
#endif /* NO_USER_ORDER */

	/* Exclude based on -1/--one-file-system */
	if (ISFLAG(flags, F_ONEFS) && (file1->device != file2->device)) return -4;

	 /* Exclude files by permissions if requested */
	if (ISFLAG(flags, F_PERMISSIONS) &&
		(file1->mode != file2->mode
#ifndef NO_PERMS
		|| file1->uid != file2->uid
		|| file1->gid != file2->gid
#endif
	)) return -5;

	/* Hard link and symlink + '-s' check */
#ifndef NO_HARDLINKS
	if ((file1->inode == file2->inode) && (file1->device == file2->device)) {
		if (ISFLAG(flags, F_CONSIDERHARDLINKS)) return 2;
		else return -2;
	}
#endif

	/* Fall through: all checks passed */
	return 0;
}


/* Check for exclusion conditions for a single file (1 = fail) */
int check_singlefile(file_t * const restrict newfile)
{
	char * restrict tp = tempname;

	DBG(if (unlikely(newfile == NULL)) jc_nullptr("check_singlefile()");)

	/* Exclude hidden files if requested */
	if (likely(ISFLAG(flags, F_EXCLUDEHIDDEN))) {
		if (unlikely(newfile->d_name == NULL)) jc_nullptr("check_singlefile newfile->d_name");
		strcpy(tp, newfile->d_name);
		tp = basename(tp);
		if (tp[0] == '.' && jc_streq(tp, ".") && jc_streq(tp, "..")) return 1;
	}

	/* Get file information and check for validity */
	const int i = getfilestats(newfile);

	if (i || newfile->size == -1) return 1;

	if (!JC_S_ISREG(newfile->mode) && !JC_S_ISDIR(newfile->mode)) return 1;

	if (!JC_S_ISDIR(newfile->mode)) {
		/* Exclude zero-length files if requested */
		if (newfile->size == 0 && !ISFLAG(flags, F_INCLUDEEMPTY)) return 1;
#ifndef NO_EXTFILTER
		if (extfilter_exclude(newfile)) return 1;
#endif /* NO_EXTFILTER */
	}

#ifdef ON_WINDOWS
	/* Windows has a 1023 (+1) hard link limit. If we're hard linking,
	 * ignore all files that have hit this limit */
 #ifndef NO_HARDLINKS
	if (ISFLAG(a_flags, FA_HARDLINKFILES) && newfile->nlink >= 1024) {
		DBG(hll_exclude++;)
		return 1;
	}
 #endif /* NO_HARDLINKS */
#endif /* ON_WINDOWS */
	return 0;
}
