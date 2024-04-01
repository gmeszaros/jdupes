/* jdupes duplicate file finder utility
 * Copyright (C) 2015-2024 by Jody Bruchon <jody@jodybruchon.com>
 * Licensed under The MIT License; see LICENSE.txt for details */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#ifndef NO_GETOPT_LONG
 #include <getopt.h>
#endif
#include <errno.h>

#include <libjodycode.h>
#include "libjodycode_check.h"

#include "likely_unlikely.h"
#include "jdupes.h"
#include "checks.h"
#ifndef NO_EXTFILTER
 #include "extfilter.h"
#endif
#include "filehash.h"
#include "filestat.h"
#ifndef NO_HASHDB
 #include "hashdb.h"
#endif
#include "helptext.h"
#include "interrupt.h"
#include "loaddir.h"
#include "match.h"
#include "progress.h"
#include "sizetree.h"
#include "sort.h"
#ifndef NO_TRAVCHECK
 #include "travcheck.h"
#endif
#include "query.h"
#include "version.h"

#ifndef USE_JODY_HASH
 #include "xxhash.h"
#endif

/* Headers for post-scanning actions */
#include "act_deletefiles.h"
#ifdef ENABLE_DEDUPE
 #include "act_dedupefiles.h"
#endif
#include "act_linkfiles.h"
#include "act_printmatches.h"
#ifndef NO_JSON
 #include "act_printjson.h"
#endif /* NO_JSON */
#include "act_summarize.h"


/* Detect Windows and modify as needed */
#if defined _WIN32 || defined __MINGW32__
 #ifdef UNICODE
 #endif /* UNICODE */
#else /* Not Windows */
 #ifdef UNICODE
	#error Do not define UNICODE on non-Windows platforms.
	#undef UNICODE
 #endif
#endif /* _WIN32 || __MINGW32__ */

/* Behavior modification flags (a=action, p=-P) */
uint32_t flags = 0;
uint32_t a_flags = 0;
enum e_printflags printflags = PF_NONE;

static const char *program_name;

#ifndef PARTIAL_HASH_SIZE
 #define PARTIAL_HASH_SIZE 4096
#endif

#ifndef NO_CHUNKSIZE
 size_t auto_chunk_size = CHUNK_SIZE;
#else
 /* If automatic chunk sizing is disabled, just use a fixed value */
 #define auto_chunk_size CHUNK_SIZE
#endif /* NO_CHUNKSIZE */

/* Required for progress indicator code */
uintmax_t filecount = 0, progress = 0, item_progress = 0, dupecount = 0;

/* Hash algorithm (see filehash.h) */
#ifdef USE_JODY_HASH
int hash_algo = HASH_ALGO_JODYHASH64;
#else
int hash_algo = HASH_ALGO_XXHASH2_64;
#endif

#ifndef NO_MTIME  /* Remove if new order types are added! */
	ordertype_t ordertype = ORDER_NAME;
#endif

/* Directory/file parameter position counter */
unsigned int user_item_count = 1;

/* Sort order reversal */
int sort_direction = 1;

/* For path name mangling */
char tempname[PATHBUF_SIZE * 2];

/* Parameter prefix string length */
int *paramprefix;
int paramprefixcnt = 0;

/* Strings used in multiple places */
const char *s_interrupt = "\nStopping file scan due to user abort\n";
const char *s_no_dupes = "No duplicates found.\n";

/* Exit status; use exit() codes for setting this */
int exit_status = EXIT_SUCCESS;

/***** End definitions, begin code *****/

/***** Add new functions here *****/


#ifndef NO_USER_ORDER
void add_param_prefix(char *param) {
	const char *func_name = "paramprefix";

	if (param == NULL) jc_nullptr(func_name);
	paramprefix = (int *)realloc(paramprefix, sizeof(int) * (paramprefixcnt + 1));
	if (paramprefix == NULL) jc_oom(func_name);
	paramprefix[paramprefixcnt] = strlen(param);
	paramprefixcnt++;
	return;
}
#endif /* NO_USER_ORDER */


#ifdef UNICODE
int wmain(int argc, wchar_t **wargv)
#else
int main(int argc, char **argv)
#endif
{
	static file_t *files = NULL;
	static file_t *curfile;
	static int opt;
	static int pm = 1;
	static int partialonly_spec = 0;
#ifndef NO_CHUNKSIZE
	static long manual_chunk_size = 0;
 #ifdef __linux__
	struct jc_proc_cacheinfo *pci;
 #endif /* __linux__ */
#endif /* NO_CHUNKSIZE */
#ifndef NO_HASHDB
	char *hashdb_name = NULL;
	int hdblen;
	int64_t hdbsize;
	uint64_t hdbout;
#endif

#ifndef NO_GETOPT_LONG
	static const struct option long_options[] =
	{
		{ "print-null", 0, 0, '0' },
		{ "one-file-system", 0, 0, '1' },
		{ "no-hidden", 0, 0, 'A' },
		{ "dedupe", 0, 0, 'B' },
		{ "chunk-size", 1, 0, 'C' },
		{ "delete", 0, 0, 'd' },
		{ "error-on-dupe", 0, 0, 'e' },
		{ "ext-option", 0, 0, 'E' },
		{ "omit-first", 0, 0, 'f' },
		{ "hard-links", 0, 0, 'H' },
		{ "help", 0, 0, 'h' },
		{ "isolate", 0, 0, 'I' },
		{ "reverse", 0, 0, 'i' },
		{ "json", 0, 0, 'j' },
/*    { "skip-hash", 0, 0, 'K' }, */
		{ "link-hard", 0, 0, 'L' },
		{ "link-soft", 0, 0, 'l' },
		{ "print-summarize", 0, 0, 'M'},
		{ "summarize", 0, 0, 'm'},
		{ "no-prompt", 0, 0, 'N' },
		{ "param-order", 0, 0, 'O' },
		{ "order", 1, 0, 'o' },
		{ "print", 1, 0, 'P' },
		{ "permissions", 0, 0, 'p' },
		{ "quick", 0, 0, 'Q' },
		{ "quiet", 0, 0, 'q' },
		{ "recurse", 0, 0, 'r' },
		{ "size", 0, 0, 'S' },
		{ "symlinks", 0, 0, 's' },
		{ "partial-only", 0, 0, 'T' },
		{ "no-change-check", 0, 0, 't' },
		{ "no-trav-check", 0, 0, 'U' },
		{ "print-unique", 0, 0, 'u' },
		{ "version", 0, 0, 'v' },
		{ "ext-filter", 1, 0, 'X' },
		{ "hash-db", 1, 0, 'y' },
		{ "soft-abort", 0, 0, 'Z' },
		{ "zero-match", 0, 0, 'z' },
		{ NULL, 0, 0, 0 }
	};
 #define GETOPT getopt_long
#else
 #define GETOPT getopt
#endif

#define GETOPT_STRING "01ABC:dEefHhIijKLlMmNnOo:P:pQqrSsTtUuVvX:y:Zz"

	/* Verify libjodycode compatibility before going further */
	if (libjodycode_version_check(1, 0) != 0) {
		version_text(1);
		exit(EXIT_FAILURE);
	}

/* Windows buffers our stderr output; don't let it do that */
#ifdef ON_WINDOWS
	if (setvbuf(stderr, NULL, _IONBF, 0) != 0)
		fprintf(stderr, "warning: setvbuf() failed\n");
#endif

#ifdef UNICODE
	/* Create a UTF-8 **argv from the wide version */
	static char **argv;
	int wa_err;
	argv = (char **)malloc(sizeof(char *) * (size_t)argc);
	if (!argv) jc_oom("main() unicode argv");
	wa_err = jc_widearg_to_argv(argc, wargv, argv);
	if (wa_err != 0) {
		jc_print_error(wa_err);
		exit(EXIT_FAILURE);
	}
	/* fix up __argv so getopt etc. don't crash */
	__argv = argv;
	jc_set_output_modes(JC_MODE_UTF16_TTY, JC_MODE_UTF16_TTY);
#endif /* UNICODE */

#ifndef NO_CHUNKSIZE
#ifdef __linux__
	/* Auto-tune chunk size to be half of L1 data cache if possible */
	pci = jc_get_proc_cacheinfo(0);
	if (pci != NULL) {
		if (pci->l1 != 0) auto_chunk_size = (pci->l1 / 2);
		else if (pci->l1d != 0) auto_chunk_size = (pci->l1d / 2);
		jc_get_proc_cacheinfo(1);
	}
	/* Must be at least 4096 (4 KiB) and cannot exceed CHUNK_SIZE */
	if (auto_chunk_size < MIN_CHUNK_SIZE || auto_chunk_size > MAX_CHUNK_SIZE) auto_chunk_size = CHUNK_SIZE;
	/* Force to a multiple of 4096 if it isn't already */
	if ((auto_chunk_size & 0x00000fffUL) != 0)
		auto_chunk_size = (auto_chunk_size + 0x00000fffUL) & 0x000ff000;
#endif /* __linux__ */
#endif /* NO_CHUNKSIZE */

	/* Is stderr a terminal? If not, we won't write progress to it */
#ifdef ON_WINDOWS
	if (!_isatty(_fileno(stderr))) SETFLAG(flags, F_HIDEPROGRESS);
#else
	if (!isatty(fileno(stderr))) SETFLAG(flags, F_HIDEPROGRESS);
#endif

	program_name = argv[0];

	while ((opt = GETOPT(argc, argv, GETOPT_STRING
#ifndef NO_GETOPT_LONG
					, long_options, NULL
#endif
				 )) != EOF) {
		if ((uintptr_t)optarg == 0x20) goto error_optarg;
		switch (opt) {
		case '0':
			SETFLAG(a_flags, FA_PRINTNULL);
			break;
		case '1':
			SETFLAG(flags, F_ONEFS);
			break;
		case 'A':
			SETFLAG(flags, F_EXCLUDEHIDDEN);
			break;
#ifdef ENABLE_DEDUPE
		case 'B':
#ifdef __linux__
			/* Kernel-level dedupe will do the byte-for-byte check itself */
			if (!ISFLAG(flags, F_PARTIALONLY)) SETFLAG(flags, F_QUICKCOMPARE);
#endif /* __linux__ */
			SETFLAG(a_flags, FA_DEDUPEFILES);
			/* It is completely useless to dedupe zero-length extents */
			CLEARFLAG(flags, F_INCLUDEEMPTY);
			break;
#endif /* ENABLE_DEDUPE */
#ifndef NO_CHUNKSIZE
		case 'C':
			manual_chunk_size = (strtol(optarg, NULL, 10) & 0x0ffffffcL) << 10;  /* Align to 4K sizes */
			if (manual_chunk_size < MIN_CHUNK_SIZE || manual_chunk_size > MAX_CHUNK_SIZE) {
				fprintf(stderr, "warning: invalid manual chunk size (must be %d - %d KiB); using defaults\n", MIN_CHUNK_SIZE / 1024, MAX_CHUNK_SIZE / 1024);
				manual_chunk_size = 0;
			} else auto_chunk_size = (size_t)manual_chunk_size;
			break;
#endif /* NO_CHUNKSIZE */
#ifndef NO_DELETE
		case 'd':
			SETFLAG(a_flags, FA_DELETEFILES);
			break;
#endif /* NO_DELETE */
#ifndef NO_ERRORONDUPE
		case 'E':
			fprintf(stderr, "The -E option has been moved to -e as threatened in 1.26.1!\n");
			fprintf(stderr, "Fix whatever used -E and try again. This is not a bug. Exiting.\n");
			exit(EXIT_FAILURE);
			break;
		case 'e':
			SETFLAG(a_flags, FA_ERRORONDUPE);
			break;
#endif /* NO_ERRORONDUPE */
		case 'f':
			SETFLAG(a_flags, FA_OMITFIRST);
			break;
		case 'h':
			help_text();
			exit(EXIT_SUCCESS);
#ifndef NO_HARDLINKS
		case 'H':
			SETFLAG(flags, F_CONSIDERHARDLINKS);
			break;
		case 'L':
			SETFLAG(a_flags, FA_HARDLINKFILES);
			break;
#endif
		case 'i':
			SETFLAG(flags, F_REVERSESORT);
			break;
#ifndef NO_USER_ORDER
		case 'I':
			SETFLAG(flags, F_ISOLATE);
			break;
		case 'O':
			SETFLAG(flags, F_USEPARAMORDER);
			break;
#else
		case 'I':
		case 'O':
			fprintf(stderr, "warning: -I and -O are disabled and ignored in this build\n");
			break;
#endif
#ifndef NO_JSON
		case 'j':
			SETFLAG(a_flags, FA_PRINTJSON);
			break;
#endif /* NO_JSON */
		case 'K':
			SETFLAG(flags, F_SKIPHASH);
			break;
		case 'm':
			SETFLAG(a_flags, FA_SUMMARIZEMATCHES);
			break;
		case 'M':
			SETFLAG(a_flags, FA_SUMMARIZEMATCHES);
			SETFLAG(a_flags, FA_PRINTMATCHES);
			break;
#ifndef NO_DELETE
		case 'N':
			SETFLAG(flags, F_NOPROMPT);
			break;
#endif /* NO_DELETE */
		case 'o':
#ifndef NO_MTIME  /* Remove if new order types are added! */
			if (!jc_strncaseeq("name", optarg, 5)) {
				ordertype = ORDER_NAME;
			} else if (!jc_strncaseeq("time", optarg, 5)) {
				ordertype = ORDER_TIME;
			} else {
				fprintf(stderr, "invalid value for --order: '%s'\n", optarg);
				exit(EXIT_FAILURE);
			}
#endif /* NO_MTIME */
			break;
		case 'p':
			SETFLAG(flags, F_PERMISSIONS);
			break;
		case 'P':
			if (jc_streq(optarg, "partial") == 0) printflags = PF_PARTIAL;
			else if (jc_streq(optarg, "early") == 0) printflags = PF_EARLYMATCH;
			else if (jc_streq(optarg, "fullhash") == 0) printflags = PF_FULLHASH;
			else {
				fprintf(stderr, "Option '%s' is not valid for -P\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'q':
			SETFLAG(flags, F_HIDEPROGRESS);
			break;
		case 'Q':
			SETFLAG(flags, F_QUICKCOMPARE);
			break;
		case 'r':
			SETFLAG(flags, F_RECURSE);
			break;
		case 't':
			SETFLAG(flags, F_NOCHANGECHECK);
			break;
		case 'T':
			partialonly_spec++;
			if (partialonly_spec == 1) {
			}
			if (partialonly_spec == 2) {
				SETFLAG(flags, F_PARTIALONLY);
				CLEARFLAG(flags, F_QUICKCOMPARE);
			}
			break;
		case 'u':
			SETFLAG(a_flags, FA_PRINTUNIQUE);
			break;
		case 'U':
			SETFLAG(flags, F_NOTRAVCHECK);
			break;
		case 'v':
		case 'V':
			version_text(0);
			exit(EXIT_SUCCESS);
#ifndef NO_SYMLINKS
		case 'l':
			SETFLAG(a_flags, FA_MAKESYMLINKS);
			break;
		case 's':
			SETFLAG(flags, F_FOLLOWLINKS);
			break;
#endif
		case 'S':
			SETFLAG(a_flags, FA_SHOWSIZE);
			break;
#ifndef NO_EXTFILTER
		case 'X':
			add_extfilter(optarg);
			break;
#endif /* NO_EXTFILTER */
#ifndef NO_HASHDB
		case 'y':
			SETFLAG(flags, F_HASHDB);
			fprintf(stderr, "\nWARNING: THE HASH DATABASE FEATURE IS UNDER HEAVY DEVELOPMENT! It functions\n");
			fprintf(stderr,   "         but there are LOTS OF QUIRKS. The behavior is not fully documented\n");
			fprintf(stderr,   "         yet and basic 'smarts' have not been implemented. USE THIS FEATURE\n");
			fprintf(stderr,   "         AT YOUR OWN RISK. Report hashdb issues to jody@jodybruchon.com\n\n");
			hdbsize = 0;
			hdblen = strlen(optarg) + 1;
			if (hdblen < 24) hdblen = 24;
			hashdb_name = (char *)malloc(hdblen);
			if (hashdb_name == NULL) jc_oom("hashdb_name alloc");
			if (strcmp(optarg, ".") == 0) strcpy(hashdb_name, "jdupes_hashdb.txt");
			else strcpy(hashdb_name, optarg);
			break;
#endif /* NO_HASHDB */
		case 'z':
			SETFLAG(flags, F_INCLUDEEMPTY);
			break;
		case 'Z':
			SETFLAG(flags, F_SOFTABORT);
			break;

		default:
			if (opt != '?') fprintf(stderr, "Sorry, using '-%c' is not supported in this build.\n", opt);
			fprintf(stderr, "Try `jdupes --help' for more information.\n");
			exit(EXIT_FAILURE);
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "no files or directories specified (use -h option for help)\n");
		exit(EXIT_FAILURE);
	}

	/* Make noise if people try to use -T because it's super dangerous */
	if (partialonly_spec > 0) {
		if (partialonly_spec > 2) {
			if (!ISFLAG(flags, F_HIDEPROGRESS)) fprintf(stderr, "Saying -T three or more times? You're a wizard. No reminders for you.\n");
			goto skip_partialonly_noise;
		}
		fprintf(stderr, "\nBIG FAT WARNING: -T/--partial-only is EXTREMELY DANGEROUS! Read the manual!\n");
		fprintf(stderr,   "                 If used with destructive actions YOU WILL LOSE DATA!\n");
		fprintf(stderr,   "                 YOU ARE ON YOUR OWN. Use this power carefully.\n\n");
		if (partialonly_spec == 1) {
			fprintf(stderr, "-T is so dangerous that you must specify it twice to use it. By doing so,\n");
			fprintf(stderr, "you agree that you're OK with LOSING ALL OF YOUR DATA BY USING -T.\n\n");
			exit(EXIT_FAILURE);
		}
		if (partialonly_spec == 2) {
			fprintf(stderr, "You passed -T twice. I hope you know what you're doing. Last chance!\n\n");
			fprintf(stderr, "          HIT CTRL-C TO ABORT IF YOU AREN'T CERTAIN!\n          ");
			for (int countdown = 10; countdown > 0; countdown--) {
				fprintf(stderr, "%d, ", countdown);
				sleep(1);
			}
			fprintf(stderr, "bye-bye, data, it was nice knowing you.\n");
			fprintf(stderr, "For wizards: three tees is the way to be.\n\n");
		}
	}
skip_partialonly_noise:

	if (ISFLAG(a_flags, FA_SUMMARIZEMATCHES) && ISFLAG(a_flags, FA_DELETEFILES)) {
		fprintf(stderr, "options --summarize and --delete are not compatible\n");
		exit(EXIT_FAILURE);
	}

#if defined ENABLE_DEDUPE && !defined NO_HARDLINKS
	if (ISFLAG(flags, F_CONSIDERHARDLINKS) && ISFLAG(a_flags, FA_DEDUPEFILES))
		fprintf(stderr, "warning: option --dedupe overrides the behavior of --hardlinks\n");
#endif

	/* If pm == 0, call printmatches() */
	pm = !!ISFLAG(a_flags, FA_SUMMARIZEMATCHES) +
			!!ISFLAG(a_flags, FA_DELETEFILES) +
			!!ISFLAG(a_flags, FA_HARDLINKFILES) +
			!!ISFLAG(a_flags, FA_MAKESYMLINKS) +
			!!ISFLAG(a_flags, FA_PRINTJSON) +
			!!ISFLAG(a_flags, FA_PRINTUNIQUE) +
			!!ISFLAG(a_flags, FA_ERRORONDUPE) +
			!!ISFLAG(a_flags, FA_DEDUPEFILES);

	if (pm > 1) {
			fprintf(stderr, "Only one of --summarize, --print-summarize, --delete, --link-hard,\n--link-soft, --json, --error-on-dupe, or --dedupe may be used\n");
			exit(EXIT_FAILURE);
	}
	if (pm == 0) SETFLAG(a_flags, FA_PRINTMATCHES);

#ifndef ON_WINDOWS
	/* Catch SIGUSR1 and use it to enable -Z */
	signal(SIGUSR1, catch_sigusr1);
#endif

	/* Catch CTRL-C */
	signal(SIGINT, catch_interrupt);

#ifndef NO_HASHDB
	if (ISFLAG(flags, F_HASHDB)) {
		hdbsize = load_hash_database(hashdb_name);
		if (hdbsize < 0) goto error_load_hashdb;
		if (hdbsize > 0 && !ISFLAG(flags, F_HIDEPROGRESS)) fprintf(stderr, "%" PRId64 " entries loaded.\n", hdbsize);
	}
#endif /* NO_HASHDB */

	/* Progress indicator every second */
	if (!ISFLAG(flags, F_HIDEPROGRESS)) {
		jc_start_alarm(1, 1);
		/* Force an immediate progress update */
		jc_alarm_ring = 1;
	}

#ifndef NO_USER_ORDER
	paramprefix = (int *)malloc(sizeof(int));
	if (paramprefix == NULL) jc_oom("paramprefix");
#endif /* NO_USER_ORDER */
	for (int x = optind; x < argc; x++) {
		if (unlikely(interrupt)) goto interrupt_exit;
		loaddir(argv[x], ISFLAG(flags, F_RECURSE));
#ifndef NO_USER_ORDER
		add_param_prefix(argv[x]);
#endif /* NO_USER_ORDER */
		user_item_count++;
	}

	/* Abort on CTRL-C (-Z doesn't matter yet) */
	if (unlikely(interrupt)) goto interrupt_exit;

	/* Force a progress update */
	if (!ISFLAG(flags, F_HIDEPROGRESS)) update_phase1_progress("items");

/* We don't need the double traversal check tree anymore */
#ifndef NO_TRAVCHECK
	travcheck_free(NULL);
#endif /* NO_TRAVCHECK */

	if (ISFLAG(flags, F_REVERSESORT)) sort_direction = -1;
	if (!ISFLAG(flags, F_HIDEPROGRESS)) fprintf(stderr, "\n");

	/* Force an immediate progress update */
	progress = 0;
	if (!ISFLAG(flags, F_HIDEPROGRESS)) jc_alarm_ring = 1;

	/* Populate size tree with matches */
	st_state_t *st_state = sizetree_new_state();
	for (curfile = sizetree_next_list(st_state); curfile != NULL; curfile = sizetree_next_list(st_state)) {
		while (curfile != NULL) {
			if (unlikely(interrupt != 0)) {
				if (!ISFLAG(flags, F_SOFTABORT)) exit(EXIT_FAILURE);
				interrupt = 0;  /* reset interrupt for re-use */
				goto skip_file_scan;
			}

			for (file_t *scanfile = curfile->next; scanfile != NULL; scanfile = scanfile->next) {
				int match;

				match = checkmatch(curfile, scanfile);
				if (match == 0) {
					/* Quick or partial-only compare will never run confirmmatch()
					 * Also skip match confirmation for hard-linked files
					 * (This set of comparisons is ugly, but quite efficient) */
					if ( ISFLAG(flags, F_QUICKCOMPARE)
						|| ISFLAG(flags, F_PARTIALONLY)
	#ifndef NO_HARDLINKS
						|| (ISFLAG(flags, F_CONSIDERHARDLINKS)
						&&  (curfile->inode == scanfile->inode)
						&&  (curfile->device == scanfile->device))
	#endif
							) {
						goto register_pair;
					}

					if (confirmmatch(curfile->d_name, scanfile->d_name, curfile->size) != 0) {
						goto skip_register;
					}
				} else goto skip_register;

register_pair:
				registerpair(curfile, scanfile);
				dupecount++;

skip_register:
#if 0
				while (scanfile != NULL && ISFLAG(scanfile->flags, FF_HAS_DUPES) && scanfile->next != NULL) {
					scanfile = scanfile->next;
				}
#endif
			} /* Scan loop end */

			check_sigusr1();
			if (jc_alarm_ring != 0) {
				jc_alarm_ring = 0;
				update_phase2_progress(NULL, -1);
			}
			progress++;

			curfile = curfile->next;
		}
	}

	if (!ISFLAG(flags, F_HIDEPROGRESS)) fprintf(stderr, "\r%60s\r", " ");

skip_file_scan:
	/* Stop catching CTRL+C and firing alarms */
	signal(SIGINT, SIG_DFL);
	if (!ISFLAG(flags, F_HIDEPROGRESS)) jc_stop_alarm();

	sizetree_free_state(st_state);

#ifndef NO_DELETE
	if (ISFLAG(a_flags, FA_DELETEFILES)) {
		if (ISFLAG(flags, F_NOPROMPT)) deletefiles(files, 0, 0);
		else deletefiles(files, 1, stdin);
	}
#endif /* NO_DELETE */
#ifndef NO_SYMLINKS
	if (ISFLAG(a_flags, FA_MAKESYMLINKS)) linkfiles(files, 0, 0);
#endif /* NO_SYMLINKS */
#ifndef NO_HARDLINKS
	if (ISFLAG(a_flags, FA_HARDLINKFILES)) linkfiles(files, 1, 0);
#endif /* NO_HARDLINKS */
#ifdef ENABLE_DEDUPE
	if (ISFLAG(a_flags, FA_DEDUPEFILES)) dedupefiles(files);
#endif /* ENABLE_DEDUPE */
	if (ISFLAG(a_flags, FA_PRINTMATCHES)) printmatches(NULL);
	if (ISFLAG(a_flags, FA_PRINTUNIQUE)) printunique();
#ifndef NO_JSON
	if (ISFLAG(a_flags, FA_PRINTJSON)) printjson(files, argc, argv);
#endif /* NO_JSON */
	if (ISFLAG(a_flags, FA_SUMMARIZEMATCHES)) {
		if (ISFLAG(a_flags, FA_PRINTMATCHES)) printf("\n\n");
		summarizematches(files);
	}

#ifndef NO_HASHDB
	if (ISFLAG(flags, F_HASHDB)) {
		hdbout = save_hash_database(hashdb_name, 1);
		if (!ISFLAG(flags, F_HIDEPROGRESS)) {
			if (hdbout > 0) fprintf(stderr, "Wrote %" PRIu64 " entries to the hash database\n", hdbout);
			else fprintf(stderr, "Hash database is OK (no changes)\n");
		}
	}
	if (hashdb_name != NULL) free(hashdb_name);
#endif

	free(paramprefix);

	exit(exit_status);

error_optarg:
	fprintf(stderr, "error: option '%c' requires an argument\n", opt);
	exit(EXIT_FAILURE);
#ifndef NO_HASHDB
error_load_hashdb:
	free(hashdb_name);
	exit(EXIT_FAILURE);
#endif
interrupt_exit:
	fprintf(stderr, "%s", s_interrupt);
	exit(EXIT_FAILURE);
}
