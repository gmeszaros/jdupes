/* File hash database management
 * This file is part of jdupes; see jdupes.c for license information */

#ifndef JDUPES_HASHDB_H
#define JDUPES_HASHDB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "jdupes.h"

typedef struct _hashdb {
  struct _hashdb *left;
  struct _hashdb *right;
  uint64_t path_hash;
  uint64_t partial_hash;
  uint64_t full_hash;
  char *path;
  time_t mtime;
  int hashcount;
} hashdb_t;

extern hashdb_t *alloc_hashdb_entry(uint64_t path_hash, int pathlen, file_t *check);
extern void dump_hashdb(hashdb_t *cur);
extern int load_hash_database(char *dbname);
extern void load_hashdb_entry(file_t *file);
extern void hd16(char *a);

#ifdef __cplusplus
}
#endif

#endif /* JDUPES_HASHDB_H */
