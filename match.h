/* jdupes file matching functions
 * This file is part of jdupes; see jdupes.c for license information */

#ifndef JDUPES_MATCH_H
#define JDUPES_MATCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include "jdupes.h"

void registerpair(file_t *file1, file_t *file2);
int checkmatch(file_t * restrict file1, file_t * const restrict file2);
int confirmmatch(const char * const restrict file1, const char * const restrict file2, const off_t size);

#ifdef __cplusplus
}
#endif

#endif /* JDUPES_MATCH_H */
