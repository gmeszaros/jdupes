/* jdupes directory scanning code
 * This file is part of jdupes; see jdupes.c for license information */

#include <dirent.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <libjodycode.h>
#include "likely_unlikely.h"
#include "jdupes.h"
#include "checks.h"
#include "extfilter.h"
#include "filestat.h"
#ifndef NO_HASHDB
 #include "hashdb.h"
#endif
#include "interrupt.h"
#include "progress.h"
#include "sizetree.h"
#ifndef NO_TRAVCHECK
 #include "travcheck.h"
#endif

/* Detect Windows and modify as needed */
#if defined _WIN32 || defined __MINGW32__
 const char dir_sep = '\\';
#else /* Not Windows */
 const char dir_sep = '/';
#endif /* _WIN32 || __MINGW32__ */

static file_t *init_newfile(const size_t len)
{
	file_t * const restrict newfile = (file_t *)calloc(1, EXTEND64(sizeof(file_t) + len));

	if (unlikely(!newfile)) jc_oom("init_newfile() file structure");

	if (newfile->dirent->d_name == NULL) jc_oom("init_newfile() filename");

#ifndef NO_USER_ORDER
	newfile->user_order = user_item_count;
#endif
	newfile->stat->st_size = -1;
	newfile->duplicates = NULL;
	return newfile;
}


static void free_file(file_t *file)
{
	if (file != NULL) free(file);
	return;
}


/* Load a directory's contents, recursing as needed */
int loaddir(char * const restrict dir, int recurse)
{
	file_t * restrict newfile;
	struct JC_DIRENT *dirinfo;
	size_t dirlen, dirpos;
	int i;
//  single = 0;
	jdupes_ino_t inode;
	dev_t device, n_device;
	jdupes_mode_t mode;
	JC_DIR *cd;
	static int sf_warning = 0; /* single file warning should only appear once */

	if (unlikely(interrupt != 0)) return -1;

	/* Convert forward slashes to backslashes if on Windows */
	jc_slash_convert(dir);

#ifndef NO_EXTFILTER
	if (extfilter_path_exclude(dir)) return 0;
#endif /* NO_EXTFILTER */

	/* Get directory stats (or file stats if it's a file) */
	i = getdirstats(dir, &inode, &device, &mode);
	if (unlikely(i < 0)) goto error_stat_dir;

	/* if dir is actually a file, just add it to the file tree */
	if (i == 1) {
		if (sf_warning == 0) {
			fprintf(stderr, "\nFile specs on command line disabled in this version for safety\n");
			fprintf(stderr, "This should be restored (and safe) in a future release\n");
			fprintf(stderr, "More info at jdupes.com or email jody@jodybruchon.com\n");
			sf_warning = 1;
		}
		return -1; /* Remove when single file is restored */
	}

/* Double traversal prevention tree */
#ifndef NO_TRAVCHECK
	if (likely(!ISFLAG(flags, F_NOTRAVCHECK))) {
		i = traverse_check(device, inode);
		if (unlikely(i == 1)) return 0;
		if (unlikely(i == 2)) goto error_stat_dir;
	}
#endif /* NO_TRAVCHECK */

	item_progress++;

	cd = jc_opendir(dir);
	if (unlikely(!cd)) goto error_cd;
	dirlen = strlen(dir);

	while ((dirinfo = jc_readdir(cd)) != NULL) {
		char * restrict tp = tempname;
		size_t d_name_len;

		if (unlikely(interrupt != 0)) return -1;
		if (unlikely(!jc_streq(dirinfo->d_name, ".") || !jc_streq(dirinfo->d_name, ".."))) continue;
		check_sigusr1();
		if (jc_alarm_ring != 0) {
			jc_alarm_ring = 0;
			update_phase1_progress("dirs");
		}

		/* Assemble the file's full path name, optimized to avoid strcat() */
		dirpos = dirlen;
		d_name_len = jc_get_d_namlen(dirinfo);
		memcpy(tp, dir, dirpos + 1);
		if (dirpos != 0 && tp[dirpos - 1] != dir_sep) {
			tp[dirpos] = dir_sep;
			dirpos++;
		}
		if (unlikely(dirpos + d_name_len + 1 >= (PATHBUF_SIZE * 2))) goto error_overflow;
		tp += dirpos;
		memcpy(tp, dirinfo->d_name, d_name_len);
		tp += d_name_len;
		*tp = '\0';
		d_name_len++;

		/* Allocate the file_t and the d_name entries */
		newfile = init_newfile(dirpos + d_name_len + 2);

		tp = tempname;
		memcpy(newfile->dirent->d_name, tp, dirpos + d_name_len);

		/*** WARNING: tempname global gets reused by check_singlefile here! ***/

		/* Single-file [l]stat() and exclusion condition check */
		if (check_singlefile(newfile) != 0) {
			free_file(newfile);
			continue;
		}

		/* Optionally recurse directories, including symlinked ones if requested */
		if (JC_S_ISDIR(newfile->stat->st_mode)) {
			if (recurse) {
				/* --one-file-system - WARNING: this clobbers inode/mode */
				if (ISFLAG(flags, F_ONEFS)
						&& (getdirstats(newfile->dirent->d_name, &inode, &n_device, &mode) == 0)
						&& (device != n_device)) {
					free_file(newfile);
					continue;
				}
#ifndef NO_SYMLINKS
				else if (ISFLAG(flags, F_FOLLOWLINKS) || !ISFLAG(newfile->flags, FF_IS_SYMLINK)) {
					loaddir(newfile->dirent->d_name, recurse);
				}
#else
				else loaddir(newfile->dirent->d_name, recurse);
#endif /* NO_SYMLINKS */
			}
			free_file(newfile);
			if (unlikely(interrupt != 0)) return -1;
			continue;
		} else {
			/* Add regular files to list, including symlink targets if requested */
#ifndef NO_SYMLINKS
			if (!ISFLAG(newfile->flags, FF_IS_SYMLINK) || (ISFLAG(newfile->flags, FF_IS_SYMLINK) && ISFLAG(flags, F_FOLLOWLINKS)))
#else
			if (JC_S_ISREG(newfile->stat->st_mode))
#endif
			{
#ifndef NO_HASHDB
				if (ISFLAG(flags, F_HASHDB)) read_hashdb_entry(newfile);
#endif
				sizetree_add(newfile);
				filecount++;
				progress++;

			} else {
				free_file(newfile);
//    if (single == 1) return;
				continue;
			}
		}
		/* Skip directory stuff if adding only a single file */
//    if (single == 1) return;
	}

	jc_closedir(cd);

	return 1;

error_stat_dir:
	fprintf(stderr, "\ncould not stat dir "); jc_fwprint(stderr, dir, 1);
	exit_status = EXIT_FAILURE;
	return -1;
error_cd:
	fprintf(stderr, "\ncould not chdir to "); jc_fwprint(stderr, dir, 1);
	exit_status = EXIT_FAILURE;
	return -1;
error_overflow:
	fprintf(stderr, "\nerror: a path overflowed (longer than PATHBUF_SIZE) cannot continue\n");
	exit(EXIT_FAILURE);
}
