/* jdupes file matching functions
 * This file is part of jdupes; see jdupes.c for license information */

#ifdef __linux__
 #include <fcntl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libjodycode.h>

#include "jdupes.h"
#include "likely_unlikely.h"
#include "checks.h"
#include "filehash.h"
#ifndef NO_HASHDB
 #include "hashdb.h"
#endif
#include "interrupt.h"
#include "match.h"
#include "progress.h"


#ifndef NO_HARDLINKS
/* Copy any hashes between entries for detected hard-linked files */
static void cross_copy_hashes(file_t *file1, file_t *file2)
{
#ifndef NO_HASHDB
	int dirty1 = 0, dirty2 = 0;
#endif

	if (file1 == NULL || file2 == NULL) jc_nullptr("cross_copy_hashes()");

	if (ISFLAG(file1->flags, FF_HASH_FULL)) {
		if (ISFLAG(file2->flags, FF_HASH_FULL)) return;
		file2->filehash_partial = file1->filehash_partial;
		file2->filehash = file1->filehash;
		SETFLAG(file2->flags, FF_HASH_PARTIAL | FF_HASH_FULL);
#ifndef NO_HASHDB
		dirty2 = 1;
#endif
	} else if (ISFLAG(file2->flags, FF_HASH_FULL)) {
		if (ISFLAG(file1->flags, FF_HASH_FULL)) return;
		file1->filehash_partial = file2->filehash_partial;
		file1->filehash = file2->filehash;
		SETFLAG(file1->flags, FF_HASH_PARTIAL | FF_HASH_FULL);
#ifndef NO_HASHDB
		dirty1 = 1;
#endif
	} else if (ISFLAG(file1->flags, FF_HASH_PARTIAL)) {
		if (ISFLAG(file2->flags, FF_HASH_PARTIAL)) return;
		file2->filehash_partial = file1->filehash_partial;
		SETFLAG(file2->flags, FF_HASH_PARTIAL);
#ifndef NO_HASHDB
		dirty2 = 1;
#endif
	} else if (ISFLAG(file2->flags, FF_HASH_PARTIAL)) {
		if (ISFLAG(file1->flags, FF_HASH_PARTIAL)) return;
		file1->filehash_partial = file2->filehash_partial;
		SETFLAG(file1->flags, FF_HASH_PARTIAL);
#ifndef NO_HASHDB
		dirty1 = 1;
#endif
	}

	/* Add to hash database */
#ifndef NO_HASHDB
	if (ISFLAG(flags, F_HASHDB)) {
		if (dirty1 == 1) add_hashdb_entry(NULL, 0, file1);
		if (dirty2 == 1) add_hashdb_entry(NULL, 0, file2);
 }
#endif

	return;
}
#endif  /* NO_HARDLINKS */


void registerpair(file_t *file1, file_t *file2)
{
	file_t *scan, *src = NULL, *dest = NULL;
	int compare;
	(void)compare;

#ifndef NO_ERRORONDUPE
	if (ISFLAG(a_flags, FA_ERRORONDUPE)) {
		if (!ISFLAG(flags, F_HIDEPROGRESS)) fprintf(stderr, "\r");
		fprintf(stderr, "Exiting based on user request (-e); duplicates found:\n");
		printf("%s\n%s\n", file1->dirent->d_name, file2->dirent->d_name);
		exit(255);
	}
#endif

	if (ISFLAG(file1->flags, FF_HAS_DUPES) && ISFLAG(file2->flags, FF_HAS_DUPES)) {
		// No action needed if both files have dupes and chain_heads are identical
		if (file1->chain_head == file2->chain_head) return;

		// If both have dupes but chain_heads are different then merge the chains
		for (scan = file1; scan->duplicates != NULL; scan = scan->duplicates);
		scan->duplicates = file2->chain_head;
		CLEARFLAG(file2->chain_head->flags, FF_DUPE_CHAIN_HEAD);
		for (scan = file2->chain_head; scan != NULL; scan = scan->duplicates) {
			scan->chain_head = file1->chain_head;
		}
		return;
	}
	// If one file FF_HAS_DUPES but the other doesn't, insert the other into the chain
	if (ISFLAG(file1->flags, FF_HAS_DUPES)) {
		src = file1; dest = file2;
	} else if (ISFLAG(file2->flags, FF_HAS_DUPES)) {
		src = file2; dest = file1;
	}
	if (src != NULL) {
		for (scan = src; scan->duplicates != NULL; scan = scan->duplicates);
		scan->duplicates = dest;
		dest->chain_head = src->chain_head;
		SETFLAG(dest->flags, FF_HAS_DUPES);
		return;
	}
	// If neither file FF_HAS_DUPES yet then make a new chain
	SETFLAG(file1->flags, FF_HAS_DUPES | FF_DUPE_CHAIN_HEAD);
	SETFLAG(file2->flags, FF_HAS_DUPES);
	file1->chain_head = file1;
	file2->chain_head = file1;
	file1->duplicates = file2;

	return;
}


/* Check two files for a match
 * file1 should ALWAYS be the "current file" being scanned against */
int checkmatch(file_t * restrict file1, file_t * const restrict file2)
{
	int cmpresult = 0;
	const uint64_t * restrict filehash;
#ifndef NO_HASHDB
	int dirty1 = 0, dirty2 = 0;
#endif

	/* If device and inode fields are equal one of the files is a
	 * hard link to the other or the files have been listed twice
	 * unintentionally. We don't want to flag these files as
	 * duplicates unless the user specifies otherwise. */

	/* If considering hard linked files as duplicates, they are
	 * automatically duplicates without being read further since
	 * they point to the exact same inode. If we aren't considering
	 * hard links as duplicates, we just return -1. */

	cmpresult = check_conditions(file1, file2);
	switch (cmpresult) {
#ifndef NO_HARDLINKS
		case 2:
			cross_copy_hashes(file1, file2);
			return 0;  /* linked files + -H switch */
		case -2:    /* linked files, no -H switch */
			cross_copy_hashes(file1, file2);
			return -1;
#endif
		case -3:    /* user order */
		case -4:    /* one filesystem */
		case -5:    /* permissions */
		case -6:    /* already has dupes */
			return -1;
		default: break;
	}

	/* If preliminary matching succeeded, do main file data checks */
	if (cmpresult == 0) {
		/* Attempt to exclude files quickly with partial file hashing */
		if (!ISFLAG(file1->flags, FF_HASH_PARTIAL)) {
			filehash = get_filehash(file1, PARTIAL_HASH_SIZE, hash_algo);
			if (filehash == NULL) return -1;

			file1->filehash_partial = *filehash;
			SETFLAG(file1->flags, FF_HASH_PARTIAL);
#ifndef NO_HASHDB
			dirty1 = 1;
#endif
		}

		if (!ISFLAG(file2->flags, FF_HASH_PARTIAL)) {
			filehash = get_filehash(file2, PARTIAL_HASH_SIZE, hash_algo);
			if (filehash == NULL) return -1;

			file2->filehash_partial = *filehash;
			SETFLAG(file2->flags, FF_HASH_PARTIAL);
#ifndef NO_HASHDB
			dirty2 = 1;
#endif
		}

		cmpresult = HASH_COMPARE(file1->filehash_partial, file2->filehash_partial);

		/* Print partial hash matching pairs if requested */
		if (cmpresult == 0 && printflags == PF_PARTIAL)
			printf("\nPartial hashes match:\n   %s\n   %s\n\n", file1->dirent->d_name, file2->dirent->d_name);

		if (file1->stat->st_size <= PARTIAL_HASH_SIZE) {
			/* filehash_partial = filehash if file is small enough */
			if (!ISFLAG(file1->flags, FF_HASH_FULL)) {
				file1->filehash = file1->filehash_partial;
				SETFLAG(file1->flags, FF_HASH_FULL);
#ifndef NO_HASHDB
				dirty1 = 1;
#endif
			}
			if (!ISFLAG(file2->flags, FF_HASH_FULL)) {
				file2->filehash = file2->filehash_partial;
				SETFLAG(file2->flags, FF_HASH_FULL);
#ifndef NO_HASHDB
				dirty2 = 1;
#endif
			}
		} else if (cmpresult == 0) {
			/* If partial match was correct, perform a full file hash match */
			if (!ISFLAG(file1->flags, FF_HASH_FULL)) {
				filehash = get_filehash(file1, 0, hash_algo);
				if (filehash == NULL) return -1;

				file1->filehash = *filehash;
				SETFLAG(file1->flags, FF_HASH_FULL);
#ifndef NO_HASHDB
				dirty1 = 1;
#endif
			}

			if (!ISFLAG(file2->flags, FF_HASH_FULL)) {
				filehash = get_filehash(file2, 0, hash_algo);
				if (filehash == NULL) return -1;

				file2->filehash = *filehash;
				SETFLAG(file2->flags, FF_HASH_FULL);
#ifndef NO_HASHDB
				dirty2 = 1;
#endif
			}

			/* Full file hash comparison */
			cmpresult = HASH_COMPARE(file1->filehash, file2->filehash);
		}
	}  /* if (cmpresult == 0) */

	/* Add to hash database */
#ifndef NO_HASHDB
	if (ISFLAG(flags, F_HASHDB)) {
		if (dirty1 == 1) add_hashdb_entry(NULL, 0, file1);
		if (dirty2 == 1) add_hashdb_entry(NULL, 0, file2);
 }
#endif

	if (cmpresult != 0) {
		return -1;
	} else {
		/* All compares matched */
		if (printflags == PF_FULLHASH) printf("Full hashes match:\n   %s\n   %s\n\n", file1->dirent->d_name, file2->dirent->d_name);
		return 0;
	}
}


/* Do a byte-by-byte comparison in case two different files produce the
	 same signature. Unlikely, but better safe than sorry. */
int confirmmatch(const char * const restrict file1, const char * const restrict file2, const off_t size)
{
	static char *c1 = NULL, *c2 = NULL;
	FILE *fp1, *fp2;
	size_t r1, r2;
	off_t bytes = 0;
	int retval = 0;

	if (unlikely(c1 == NULL || c2 == NULL)) {
		c1 = (char *)malloc(auto_chunk_size);
		c2 = (char *)malloc(auto_chunk_size);
	}
	if (unlikely(c1 == NULL || c2 == NULL)) jc_oom("confirmmatch() buffers");

	fp1 = jc_fopen(file1, JC_FILE_MODE_RDONLY_SEQ);
	fp2 = jc_fopen(file2, JC_FILE_MODE_RDONLY_SEQ);
	if (fp1 == NULL) {
		if (fp2 != NULL) fclose(fp2);
		goto different;
	}
	if (fp2 == NULL) {
		if (fp1 != NULL) fclose(fp1);
		goto different;
	}

	fseek(fp1, 0, SEEK_SET);
	fseek(fp2, 0, SEEK_SET);
#ifdef __linux__
	/* Tell Linux we will accees sequentially and soon */
	posix_fadvise(fileno(fp1), 0, size, POSIX_FADV_SEQUENTIAL);
	posix_fadvise(fileno(fp1), 0, size, POSIX_FADV_WILLNEED);
	posix_fadvise(fileno(fp2), 0, size, POSIX_FADV_SEQUENTIAL);
	posix_fadvise(fileno(fp2), 0, size, POSIX_FADV_WILLNEED);
#endif /* __linux__ */

	do {
		if (interrupt) goto different;
		r1 = fread(c1, sizeof(char), auto_chunk_size, fp1);
		r2 = fread(c2, sizeof(char), auto_chunk_size, fp2);

		if (r1 != r2) goto different; /* file lengths are different */
		if (memcmp (c1, c2, r1)) goto different; /* file contents are different */

		bytes += (off_t)r1;
		if (jc_alarm_ring != 0) {
			jc_alarm_ring = 0;
			update_phase2_progress("confirm", (int)((bytes * 100) / size));
		}
	} while (r2);

	/* Success: return 0 */
	goto finish_confirm;

different:
	retval = 1;

finish_confirm:
//  free(c1); free(c2);
	fclose(fp1); fclose(fp2);
	return retval;
}
