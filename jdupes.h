/* jdupes main program header
 * See jdupes.c for license information */

#ifndef JDUPES_H
#define JDUPES_H

#ifdef __cplusplus
extern "C" {
#endif

/* Detect Windows and modify as needed */
#if defined _WIN32 || defined __MINGW32__
 #ifndef ON_WINDOWS
  #define ON_WINDOWS 1
 #endif
 #define NO_SYMLINKS 1
 #define NO_PERMS 1
 #define NO_SIGACTION 1
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <io.h>
#endif /* Win32 */

#include <limits.h>
#include <stdint.h>
#include <sys/types.h>

/* Some types are different on Windows */
#if defined _WIN32 || defined __MINGW32__
 typedef uint64_t jdupes_ino_t;
 typedef uint32_t jdupes_mode_t;

#else /* Not Windows */
 #include <sys/stat.h>
 typedef ino_t jdupes_ino_t;
 typedef mode_t jdupes_mode_t;
 #ifdef UNICODE
  #error Do not define UNICODE on non-Windows platforms.
  #undef UNICODE
 #endif
#endif /* _WIN32 || __MINGW32__ */

#ifndef PATHBUF_SIZE
 #ifdef UNICODE
  #define PATHBUF_SIZE 8192
 #else
  #define PATHBUF_SIZE 4096
 #endif /* UNICODE */
#endif /* PATHBUF_SIZE */

/* Maximum path buffer size to use; must be large enough for a path plus
 * any work that might be done to the array it's stored in. PATH_MAX is
 * not always true. Read this article on the false promises of PATH_MAX:
 * http://insanecoding.blogspot.com/2007/11/pathmax-simply-isnt.html
 * Windows + Unicode needs a lot more space than UTF-8 in Linux/Mac OS X
 */
/* Complain if PATHBUF_SIZE is too small */
#ifdef PATH_MAX
 #if PATHBUF_SIZE < PATH_MAX
  #if !defined LOW_MEMORY && !defined BARE_BONES
   #warning "PATHBUF_SIZE is less than PATH_MAX"
  #endif
 #endif
#endif

#define ISFLAG(a,b) ((a & b) == b)
#define SETFLAG(a,b) (a |= b)
#define CLEARFLAG(a,b) (a &= (~b))

/* Chunk sizing */
#ifndef CHUNK_SIZE
 #define CHUNK_SIZE 65536
#endif
#ifndef NO_CHUNKSIZE
 extern size_t auto_chunk_size;
 /* Larger chunk size makes large files process faster but uses more RAM */
 #define MIN_CHUNK_SIZE 1024 * 4
 #define MAX_CHUNK_SIZE 1048576 * 1024
#else
 /* If automatic chunk sizing is disabled, just use a fixed value */
 #define auto_chunk_size CHUNK_SIZE
#endif /* NO_CHUNKSIZE */

/* Low memory option overrides */
#ifdef LOW_MEMORY
 #ifndef NO_PERMS
  #define NO_PERMS 1
 #endif
#endif

/* Compare two hashes like memcmp() */
#define HASH_COMPARE(a,b) ((a > b) ? 1:((a == b) ? 0:-1))

/* Extend an allocation length to the next 64-bit (8-byte) boundary */
#define EXTEND64(a) ((a & 0x7) > 0 ? ((a & (~0x7)) + 8) : a)

/* Behavior modification flags */
extern uint32_t flags;
extern uint32_t a_flags;
#define F_RECURSE		(1ULL << 0)
#define F_HIDEPROGRESS		(1ULL << 1)
#define F_SOFTABORT		(1ULL << 2)
#define F_FOLLOWLINKS		(1ULL << 3)
#define F_INCLUDEEMPTY		(1ULL << 4)
#define F_CONSIDERHARDLINKS	(1ULL << 5)
#define F_NOPROMPT		(1ULL << 6)
#define F_EXCLUDEHIDDEN		(1ULL << 7)
#define F_PERMISSIONS		(1ULL << 8)
#define F_EXCLUDESIZE		(1ULL << 9)
#define F_QUICKCOMPARE		(1ULL << 10)
#define F_USEPARAMORDER		(1ULL << 11)
#define F_REVERSESORT		(1ULL << 12)
#define F_ISOLATE		(1ULL << 13)
#define F_ONEFS			(1ULL << 14)
#define F_NOCHANGECHECK		(1ULL << 15)
#define F_NOTRAVCHECK		(1ULL << 16)
#define F_SKIPHASH		(1ULL << 17)
#define F_HASHDB		(1ULL << 18)

/* Action-related flags */
#define FA_PRINTMATCHES		(1U << 0)
#define FA_PRINTUNIQUE		(1U << 1)
#define FA_OMITFIRST		(1U << 2)
#define FA_SUMMARIZEMATCHES	(1U << 3)
#define FA_DELETEFILES		(1U << 4)
#define FA_SHOWSIZE		(1U << 5)
#define FA_HARDLINKFILES	(1U << 6)
#define FA_DEDUPEFILES		(1U << 7)
#define FA_MAKESYMLINKS		(1U << 8)
#define FA_PRINTNULL		(1U << 9)
#define FA_PRINTJSON		(1U << 10)
#define FA_ERRORONDUPE		(1U << 11)

/* Per-file true/false flags */
#define FF_VALID_STAT		(1U << 0)
#define FF_HASH_PARTIAL		(1U << 1)
#define FF_HASH_FULL		(1U << 2)
#define FF_DUPE_CHAIN_HEAD	(1U << 3)
#define FF_HAS_DUPES		(1U << 4)
#define FF_IS_SYMLINK		(1U << 5)
#define FF_NOT_UNIQUE		(1U << 6)

/* Extra print flags */
extern enum e_printflags { PF_NONE, PF_PARTIAL, PF_FULLHASH } printflags;

typedef enum {
	ORDER_NAME = 0,
	ORDER_TIME
} ordertype_t;

#ifndef PARTIAL_HASH_SIZE
 #define PARTIAL_HASH_SIZE 4096
#endif

/* Per-file information */
typedef struct _file {
	struct _file *chain_head;
	struct _file *duplicates;
	struct _file *next;
#ifndef NO_USER_ORDER
	unsigned int user_order; /* Order of the originating command-line parameter */
#endif
	char *d_name;
	uint64_t filehash_partial;
	uint64_t filehash;
	jdupes_ino_t inode;
	off_t size;
#ifndef NO_MTIME
	time_t mtime;
#endif
	dev_t device;
	uint32_t flags;  /* Status flags */
	jdupes_mode_t mode;
#ifndef NO_ATIME
	time_t atime;
#endif
#ifndef NO_HARDLINKS
 #ifdef ON_WINDOWS
	uint32_t nlink;  /* link count on Windows is always a DWORD */
 #else
	nlink_t nlink;
 #endif /* ON_WINDOWS */
#endif
#ifndef NO_PERMS
	uid_t uid;
	gid_t gid;
#endif
} file_t;

/* Progress indicator variables */
extern uintmax_t filecount, progress, item_progress, dupecount;

extern ordertype_t ordertype;
extern int hash_algo;
extern unsigned int user_item_count;
extern int sort_direction;
extern char tempname[];
#ifndef NO_USER_ORDER
extern int *paramprefix;
extern int paramprefixcnt;
#endif
extern const char *feature_flags[];
extern const char *s_no_dupes;
extern int exit_status;

int file_has_changed(file_t * const restrict file);

#ifdef __cplusplus
}
#endif

#endif /* JDUPES_H */
