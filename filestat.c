/* jdupes (C) 2015-2024 Jody Bruchon <jody@jodybruchon.com>

	 Permission is hereby granted, free of charge, to any person
	 obtaining a copy of this software and associated documentation files
	 (the "Software"), to deal in the Software without restriction,
	 including without limitation the rights to use, copy, modify, merge,
	 publish, distribute, sublicense, and/or sell copies of the Software,
	 and to permit persons to whom the Software is furnished to do so,
	 subject to the following conditions:

	 The above copyright notice and this permission notice shall be
	 included in all copies or substantial portions of the Software.

	 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
	 CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
	 TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
	 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include <stdio.h>
#include <libjodycode.h>
#include "jdupes.h"
#include "likely_unlikely.h"

/* Check file's stat() info to make sure nothing has changed
 * Returns 1 if changed, 0 if not changed, negative if error */
int file_has_changed(file_t * const restrict file)
{
	struct JC_STAT s;

	/* If -t/--no-change-check specified then completely bypass this code */
	if (ISFLAG(flags, F_NOCHANGECHECK)) return 0;

	if (!ISFLAG(file->flags, FF_VALID_STAT)) return -66;

	if (jc_stat(file->dirent->d_name, &s) != 0) return -2;
	if (file->stat->st_ino != s.st_ino) return 1;
	if (file->stat->st_size != s.st_size) return 1;
	if (file->stat->st_dev != s.st_dev) return 1;
	if (file->stat->st_mode != s.st_mode) return 1;
#ifndef NO_MTIME
	if (file->stat->st_mtim.tv_sec != s.st_mtim.tv_sec) return 1;
	if (file->stat->st_mtim.tv_nsec != s.st_mtim.tv_nsec) return 1;
#endif
#ifndef NO_PERMS
	if (file->stat->st_uid != s.st_uid) return 1;
	if (file->stat->st_gid != s.st_gid) return 1;
#endif
#ifndef NO_SYMLINKS
	if (lstat(file->dirent->d_name, &s) != 0) return -3;
	if ((JC_S_ISLNK(s.st_mode) > 0) ^ ISFLAG(file->flags, FF_IS_SYMLINK)) return 1;
#endif

	return 0;
}


int getfilestats(file_t * const restrict file)
{
	struct JC_STAT s;

	/* Don't stat the same file more than once */
	if (ISFLAG(file->flags, FF_VALID_STAT)) return 0;
	SETFLAG(file->flags, FF_VALID_STAT);

	if (jc_stat(file->dirent->d_name, &s) != 0) return -1;
	file->stat->st_size = s.st_size;
	file->stat->st_ino = s.st_ino;
	file->stat->st_dev = s.st_dev;
#ifndef NO_MTIME
	file->stat->st_mtim.tv_sec = s.st_mtim.tv_sec;
	file->stat->st_mtim.tv_nsec = s.st_mtim.tv_nsec;
#endif
#ifndef NO_ATIME
	file->stat->st_atim.tv_sec = s.st_atim.tv_sec;
	file->stat->st_atim.tv_nsec = s.st_atim.tv_nsec;
#endif
	file->stat->st_mode = s.st_mode;
#ifndef NO_HARDLINKS
	file->stat->st_nlink = s.st_nlink;
#endif
#ifndef NO_PERMS
	file->stat->st_uid = s.st_uid;
	file->stat->st_gid = s.st_gid;
#endif
#ifndef NO_SYMLINKS
	if (lstat(file->dirent->d_name, &s) != 0) return -1;
	if (JC_S_ISLNK(s.st_mode) > 0) SETFLAG(file->flags, FF_IS_SYMLINK);
#endif
	return 0;
}


/* Returns -1 if stat() fails, 0 if it's a directory, 1 if it's not */
int getdirstats(const char * const restrict name,
		jdupes_ino_t * const restrict inode, dev_t * const restrict dev,
		jdupes_mode_t * const restrict mode)
{
	struct JC_STAT s;

	if (jc_stat(name, &s) != 0) return -1;
	*inode = s.st_ino;
	*dev = s.st_dev;
	*mode = s.st_mode;
	if (!JC_S_ISDIR(s.st_mode)) return 1;
	return 0;
}
