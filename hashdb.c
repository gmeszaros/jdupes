/* File hash database management
 * This file is part of jdupes; see jdupes.c for license information */

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "jdupes.h"
#include "libjodycode.h"
#include "likely_unlikely.h"
#include "hashdb.h"

#define HASHDB_VER 2
#define HASHDB_MIN_VER 1
#define HASHDB_MAX_VER 2
#ifndef PH_SHIFT
 #define PH_SHIFT 12
#endif
#define SECS_TO_TIME(a,b) strftime(a, 32, "%Y-%m-%d %H:%M:%S", localtime(b));

#ifndef HT_SIZE
 #define HT_SIZE 131072
#endif
#define HT_MASK (HT_SIZE - 1)

static hashdb_t *hashdb[HT_SIZE];
static int hashdb_init = 0;
static int hashdb_algo = 0;
static int hashdb_dirty = 0;
static int new_hashdb = 0;

/* Pivot direction for rebalance */
enum pivot { PIVOT_LEFT, PIVOT_RIGHT };

static int write_hashdb_entry(FILE *db, hashdb_t *cur, uint64_t *cnt, const int destroy);
static int get_path_hash(char *path, int pathlen, uint64_t *path_hash);


#if 0
/* Hex dump 16 bytes of memory */
void hd16(char *a) {
  int i;

  printf("DUMP Hex: ");
  for (i = 0; i < 16; i++) printf("%x", a[i]);
  printf("\nDUMP ASCII: ");
  for (i = 0; i < 16; i++) printf("%c", a[i]);
  printf("\n");
  return;
}
#endif


static hashdb_t *alloc_hashdb_node(const int pathlen)
{
  int allocsize;

  allocsize = sizeof(hashdb_t) + pathlen + 1;
  return (hashdb_t *)calloc(1, EXTEND64(allocsize));
}


/* destroy = 1 will free() all nodes while saving */
int save_hash_database(const char * const restrict dbname, const int destroy)
{
  FILE *db = NULL;
  uint64_t cnt = 0;
  char *dbtemp;

  if (dbname == NULL) goto error_hashdb_null;
  LOUD(fprintf(stderr, "save_hash_database('%s')\n", dbname);)
  /* Don't save the hash database if it wasn't changed */
  if (hashdb_dirty == 0 && destroy == 0) return 0;
  if (hashdb_dirty == 1) {
    dbtemp = malloc(strlen(dbname) + 5);
    if (dbtemp == NULL) goto error_hashdb_alloc;
    strcpy(dbtemp, dbname);
    strcat(dbtemp, ".tmp");
    /* Try to remove any existing temporary database, ignoring errors */
    remove(dbtemp);
    errno = 0;
    db = fopen(dbtemp, "wb");
    if (db == NULL) goto error_hashdb_open;
  }

  if (hashdb_dirty == 1) {
    if (write_hashdb_entry(db, NULL, &cnt, destroy) != 0) goto error_hashdb_write;
    fclose(db);
    if (new_hashdb == 0) {
      errno = 0;
      if (remove(dbname) != 0) {
        if (errno != ENOENT) goto error_hashdb_remove;
      }
    }
    if (rename(dbtemp, dbname) != 0) goto error_hashdb_rename;
    LOUD(if (hashdb_dirty == 1) fprintf(stderr, "Wrote %" PRIu64 " items to hash databse '%s'\n", cnt, dbname);)
    hashdb_dirty = 0;
  }

  return cnt;

error_hashdb_null:
  fprintf(stderr, "error: internal failure: NULL pointer for hashdb\n");
  return -1;
error_hashdb_open:
  fprintf(stderr, "error: cannot open temp hashdb '%s' for writing: %s\n", dbtemp, strerror(errno));
  return -2;
error_hashdb_write:
  fprintf(stderr, "error: write failed to temp hashdb '%s': %s\n", dbtemp, strerror(errno));
  fclose(db);
  return -3;
error_hashdb_alloc:
  fprintf(stderr, "error: cannot allocate memory for temporary hashdb name\n");
  return -4;
error_hashdb_remove:
  fprintf(stderr, "error: cannot delete old hashdb '%s': %s\n", dbname, strerror(errno));
  remove(dbtemp);
  return -5;
error_hashdb_rename:
  fprintf(stderr, "error: cannot rename temporary hashdb '%s' to '%s'; leaving it alone: %s\n", dbtemp, dbname, strerror(errno));
  return -6;
}


static int write_hashdb_entry(FILE *db, hashdb_t *cur, uint64_t *cnt, const int destroy)
{
  struct timeval tm;
  int err = 0;
  static char out[PATH_MAX + 128];

  /* Write header and traverse array on first call */
  if (unlikely(cur == NULL)) {
    if (hashdb_dirty == 1) {
      gettimeofday(&tm, NULL);
      snprintf(out, PATH_MAX + 127, "jdupes hashdb:%d,%d,%08lx\n", HASHDB_VER, hash_algo, (unsigned long)tm.tv_sec);
      LOUD(fprintf(stderr, "write hashdb: %s", out);)
      errno = 0;
      fputs(out, db);
      if (errno != 0) return 1;
    }
    /* Write out each hash bucket, skipping empty buckets */
    for (int i = 0; i < HT_SIZE; i++) {
      if (hashdb[i] == NULL) continue;
      err = write_hashdb_entry(db, hashdb[i], cnt, destroy);
      if (err != 0) return err;
    }
    if (destroy == 1) {
      memset(hashdb, 0, sizeof(hashdb_t *) * HT_SIZE);
      hashdb_init = 0;
    }
    return 0;
  }

  /* Write out this node if it wasn't invalidated */
  if (hashdb_dirty == 1 && cur->hashcount != 0) {
    snprintf(out, PATH_MAX + 127, "%u,%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64 ",%s\n",
      cur->hashcount, cur->partialhash, cur->fullhash, (uint64_t)cur->mtime, (uint64_t)cur->size, (uint64_t)cur->inode, cur->path);
    (*cnt)++;
    LOUD(fprintf(stderr, "write hashdb: %s", out);)
    errno = 0;
    fputs(out, db);
    if (errno != 0) return 1;
  }

  /* Traverse the tree, propagating errors */
  if (cur->left != NULL) err = write_hashdb_entry(db, cur->left, cnt, destroy);
  if (err == 0 && cur->right != NULL) err = write_hashdb_entry(db, cur->right, cnt, destroy);
  if (destroy == 1) { free(cur); }
  return err;
}


#if 0
void dump_hashdb(hashdb_t *cur)
{
  struct timeval tm;

  if (cur == NULL) {
    gettimeofday(&tm, NULL);
    printf("jdupes hashdb:1,%d,%08lx\n", hash_algo, tm.tv_sec);
    for (int i = 0; i < HT_SIZE; i++) {
      if (hashdb[i] == NULL) continue;
      dump_hashdb(hashdb[i]);
    }
    return;
  }
  /* db line format: hashcount,partial,full,mtime,path */
#ifdef ON_WINDOWS
  if (cur->hashcount != 0) printf("%u,%016llx,%016llx,%08llx,%08llx,%016llx,%s\n",
      cur->hashcount, cur->partialhash, cur->fullhash, cur->mtime, cur->size, cur->inode, cur->path);
#else
  if (cur->hashcount != 0) printf("%u,%016lx,%016lx,%08lx,%08lx,%016lx,%s\n",
      cur->hashcount, cur->partialhash, cur->fullhash, cur->mtime, cur->size, cur->inode, cur->path);
#endif
  if (cur->left != NULL) dump_hashdb(cur->left);
  if (cur->right != NULL) dump_hashdb(cur->right);
  return;
}
#endif


static void pivot_hashdb_tree(hashdb_t **parent, hashdb_t *cur, enum pivot direction)
{
  hashdb_t *temp;

  if (direction == PIVOT_LEFT) {
    temp = cur->right;
    cur->right = cur->right->left;
    temp->left = cur;
    *parent = temp;
  } else {  // PIVOT_RIGHT
    temp = cur->left;
    cur->left = cur->left->right;
    temp->right = cur;
    *parent = temp;
  }

  return;
}


static void rebalance_hashdb_tree(hashdb_t **parent)
{
  const uint64_t center = 0x8000000000000000ULL;
  hashdb_t *cur = *parent;

  if (unlikely(cur == NULL || parent == NULL)) return;
  if (cur->left == NULL && cur->right == NULL) return;

  if (cur->left != NULL) rebalance_hashdb_tree(&(cur->left));
  if (cur->right != NULL) rebalance_hashdb_tree(&(cur->right));
  if (cur->path_hash > center) {
    /* This node might be better off to the right */
    if (cur->left != NULL && cur->left->path_hash > center) pivot_hashdb_tree(parent, cur, PIVOT_RIGHT);
  } else if (cur->path_hash < center) {
    /* This node might be better off to the left */
    if (cur->right != NULL && cur->right->path_hash < center) pivot_hashdb_tree(parent, cur, PIVOT_LEFT);
  }
  return;
}


/* in_pathlen allows use of a precomputed path length to avoid extra strlen() calls */
hashdb_t *add_hashdb_entry(char *in_path, int pathlen, const file_t *check)
{
  unsigned int bucket;
  hashdb_t *file;
  hashdb_t *cur;
  uint64_t path_hash;
  int exclude;
  int difference;
  char *path;

  /* Allocate hashdb on first use */
  if (unlikely(hashdb_init == 0)) {
    memset(hashdb, 0, sizeof(hashdb_t *) * HT_SIZE);
    hashdb_init = 1;
  }

  if (unlikely((in_path == NULL && check == NULL) || (check != NULL && check->d_name == NULL))) return NULL;

  /* Get path hash and length from supplied path; use hash to choose the bucket */
  if (in_path == NULL) {
    path = check->d_name;
    pathlen = check->d_name_len;
  } else path = in_path;
  if (pathlen == 0) pathlen = strlen(path);
  if (get_path_hash(path, pathlen, &path_hash) != 0) return NULL;
  bucket = path_hash & HT_MASK;

  if (hashdb[bucket] == NULL) {
    file = alloc_hashdb_node(pathlen);
    if (file == NULL) return NULL;
    file->path_hash = path_hash;
    hashdb[bucket] = file;
  } else {
    cur = hashdb[bucket];
    difference = 0;
    while (1) {
      /* If path is set then this entry may already exist and we need to check */
      if (check != NULL && cur->path != NULL) {
        if (cur->path_hash == path_hash && cur->pathlen == check->d_name_len && memcmp(cur->path, check->d_name, cur->pathlen) == 0) {
          /* Should we invalidate this entry? */
          exclude = 0;
          if (cur->mtime != check->mtime) exclude |= 1;
          if (cur->inode != check->inode) exclude |= 2;
          if (cur->size  != check->size)  exclude |= 4;
          if (exclude == 0) {
            if (cur->hashcount == 1 && ISFLAG(check->flags, FF_HASH_FULL)) {
              cur->hashcount = 2;
              cur->fullhash = check->filehash;
              hashdb_dirty = 1;
            }
            return cur;
          } else {
            /* Something changed; invalidate this entry */
            cur->hashcount = 0;
            hashdb_dirty = 1;
            return NULL;
          }
        }
      }
      if (cur->path_hash >= path_hash) {
        if (cur->left == NULL) {
          file = alloc_hashdb_node(pathlen);
          if (file == NULL) return NULL;
          cur->left = file;
          break;
        } else {
          cur = cur->left;
          difference--;
          continue;
        }
      } else {
        if (cur->right == NULL) {
          file = alloc_hashdb_node(pathlen);
          if (file == NULL) return NULL;
          cur->right = file;
          break;
        } else {
          cur = cur->right;
          difference++;
          continue;
        }
      }
    }
    if (difference < 0) difference = -difference;
    if (difference > 64) {
      rebalance_hashdb_tree(&(hashdb[bucket]));
    }
  }

  /* If a check entry was given then populate it */
  if (check != NULL && check->d_name != NULL && ISFLAG(check->flags, FF_HASH_PARTIAL)) {
    hashdb_dirty = 1;
    file->path_hash = path_hash;
    file->path = (char *)((uintptr_t)file + (uintptr_t)sizeof(hashdb_t));
    file->pathlen = pathlen;
    memcpy(file->path, check->d_name, pathlen + 1);
    *(file->path + pathlen) = '\0';
    file->size = check->size;
    file->inode = check->inode;
    file->mtime = check->mtime;
    file->partialhash = check->filehash_partial;
    file->fullhash = check->filehash;
    if (ISFLAG(check->flags, FF_HASH_FULL)) file->hashcount = 2;
    else file->hashcount = 1;
  } else {
    /* No check entry? Populate from passed parameters */
    file->path = (char *)((uintptr_t)file + (uintptr_t)sizeof(hashdb_t));
    file->path_hash = path_hash;
  }
  return file;
}


/* db header format: jdupes hashdb:dbversion,hashtype,update_mtime
 * db line format: hashcount,partial,full,mtime,size,inode,path */
int64_t load_hash_database(char *dbname)
{
  FILE *db;
  char line[PATH_MAX + 128];
  char buf[PATH_MAX + 128];
  char *field, *temp;
  int db_ver;
  unsigned int fixed_len;
  int64_t linenum = 1;
#ifdef LOUD_DEBUG
  time_t db_mtime;
  char date[32];
#endif /* LOUD_DEBUG */
  
  if (dbname == NULL) goto error_hashdb_null;
  LOUD(fprintf(stderr, "load_hash_database('%s')\n", dbname);)
  errno = 0;
  db = fopen(dbname, "rb");
  if (db == NULL) goto warn_hashdb_open;

  /* Read header line */
  if ((fgets(buf, PATH_MAX + 127, db) == NULL) || (ferror(db) != 0)) {
    if (errno == 0) goto warn_hashdb_open;  // empty file = make new DB
    goto error_hashdb_read;
  } else if (!ISFLAG(flags, F_HIDEPROGRESS)) fprintf(stderr, "Loading hash database...");
  field = strtok(buf, ":");
  if (strcmp(field, "jdupes hashdb") != 0) goto error_hashdb_header;
  field = strtok(NULL, ":");
  temp = strtok(field, ",");
  db_ver = (int)strtoul(temp, NULL, 10);
  temp = strtok(NULL, ",");
  hashdb_algo = (int)strtoul(temp, NULL, 10);
  temp = strtok(NULL, ",");
  /* Database mod time is currently set but not used */
  LOUD(db_mtime = (int)strtoul(temp, NULL, 16);)
  LOUD(SECS_TO_TIME(date, &db_mtime);)
  LOUD(fprintf(stderr, "hashdb header: ver %u, algo %u, mod %s\n", db_ver, hashdb_algo, date);)
  if (db_ver < HASHDB_MIN_VER || db_ver > HASHDB_MAX_VER) goto error_hashdb_version;
  if (hashdb_algo != hash_algo) goto warn_hashdb_algo;

  /* v1 has 8-byte sizes; v2 has 16-byte (4GiB+) sizes */
  fixed_len = 87;
  if (db_ver == 1) fixed_len = 71;

  /* Read database entries */
  while (1) {
    int pathlen;
    unsigned int linelen;
    int hashcount;
    uint64_t partialhash, fullhash = 0;
    time_t mtime;
    char *path;
    hashdb_t *entry;
    off_t size;
    jdupes_ino_t inode;

    errno = 0;
    if ((fgets(line, PATH_MAX + 128, db) == NULL)) {
      if (ferror(db) != 0) goto error_hashdb_read;
      break;
    }
    LOUD(fprintf(stderr, "read hashdb: %s", line);)
    strncpy(buf, line, PATH_MAX + 128);
    linenum++;
    linelen = (int64_t)strlen(buf);
    if (linelen < fixed_len + 1) goto error_hashdb_line;

    /* Split each entry into fields and
     * hashcount: 1 = partial only, 2 = partial and full */
    field = strtok(buf, ","); if (field == NULL) goto error_hashdb_line;
    hashcount = (int)strtol(field, NULL, 16);
    if (hashcount < 1 || hashcount > 2) goto error_hashdb_line;
    field = strtok(NULL, ","); if (field == NULL) goto error_hashdb_line;
    partialhash = strtoull(field, NULL, 16);
    field = strtok(NULL, ","); if (field == NULL) goto error_hashdb_line;
    if (hashcount == 2) fullhash = strtoull(field, NULL, 16);
    field = strtok(NULL, ","); if (field == NULL) goto error_hashdb_line;
    mtime = (time_t)strtoul(field, NULL, 16);
    field = strtok(NULL, ","); if (field == NULL) goto error_hashdb_line;
    size = strtoll(field, NULL, 16);
    if (size == 0) goto error_hashdb_line;
    field = strtok(NULL, ","); if (field == NULL) goto error_hashdb_line;
    inode = strtoull(field, NULL, 16);

    path = buf + fixed_len;
    path = strtok(path, "\n"); if (path == NULL) goto error_hashdb_line;
    pathlen = linelen - fixed_len - 1;
    if (pathlen > PATH_MAX) goto error_hashdb_line;
    *(path + pathlen) = '\0';

    /* Allocate and populate a tree entry */
    entry = add_hashdb_entry(path, pathlen, NULL);
    if (entry == NULL) goto error_hashdb_add;
    memcpy(entry->path, path, pathlen + 1);
    entry->mtime = mtime;
    entry->inode = inode;
    entry->size = size;
    entry->partialhash = partialhash;
    entry->fullhash = fullhash;
    entry->hashcount = hashcount;
  }

  fclose(db);
  return linenum - 1;

warn_hashdb_open:
  fprintf(stderr, "Creating a new hash database '%s'\n", dbname);
  new_hashdb = 1;
  return 0;
error_hashdb_read:
  fprintf(stderr, "error reading hash database '%s': %s\n", dbname, strerror(errno));
  fclose(db);
  return -1;
error_hashdb_header:
  fprintf(stderr, "error in header of hash database '%s'\n", dbname);
  fclose(db);
  return -2;
error_hashdb_version:
  fprintf(stderr, "error: bad db version %u in hash database '%s'\n", db_ver, dbname);
  fclose(db);
  return -3;
error_hashdb_line:
  fprintf(stderr, "\nerror: bad line %" PRId64 " in hash database '%s':\n\n%s\n\n", linenum, dbname, line);
  fclose(db);
  return -4;
error_hashdb_add:
  fprintf(stderr, "error: internal failure allocating a hashdb entry\n");
  fclose(db);
  return -5;
error_hashdb_null:
  fprintf(stderr, "error: internal failure: NULL pointer for hashdb\n");
  return -6;
warn_hashdb_algo:
  fprintf(stderr, "warning: hashdb uses a different hash algorithm than selected; not loading\n");
  fclose(db);
  return -7;
}
 

static int get_path_hash(char *path, int pathlen, uint64_t *path_hash)
{
  uint64_t aligned_path[(PATH_MAX + 8) / sizeof(uint64_t)];
  int retval;

  *path_hash = 0;
  if (pathlen < 1) pathlen = strlen(path);
  if ((uintptr_t)path & 0x0f) {
    strncpy((char *)&aligned_path, path, PATH_MAX);
    retval = jc_block_hash((uint64_t *)aligned_path, path_hash, pathlen);
  } else retval = jc_block_hash((uint64_t *)path, path_hash, pathlen);
  return retval;
}


 /* If file hash info is already present in hash database then preload those hashes */
int read_hashdb_entry(file_t *file)
{
  unsigned int bucket;
  hashdb_t *cur;
  uint64_t path_hash;
  int exclude;

  LOUD(fprintf(stderr, "read_hashdb_entry('%s')\n", file->d_name);)
  if (file == NULL || file->d_name == NULL) goto error_null;
  if (get_path_hash(file->d_name, file->d_name_len, &path_hash) != 0) goto error_path_hash;
  bucket = path_hash & HT_MASK;
  if (hashdb[bucket] == NULL) return 0;
  cur = hashdb[bucket];
  while (1) {
    if (cur->path_hash != path_hash) {
      if (path_hash < cur->path_hash) cur = cur->left;
      else cur = cur->right;
      if (cur == NULL) return 0;
      continue;
    }
    /* Found a matching path hash */
    if (cur->pathlen == file->d_name_len && memcmp(cur->path, file->d_name, cur->pathlen) == 0) {
      cur = cur->left;
      if (cur == NULL) return 0;
      continue;
    } else {
      /* Found a matching path too but check mtime */
      exclude = 0;
      if (cur->mtime != file->mtime) exclude |= 1;
      if (cur->inode != file->inode) exclude |= 2;
      if (cur->size  != file->size)  exclude |= 4;
      if (exclude != 0) {
        /* Invalidate if something has changed */
        cur->hashcount = 0;
        hashdb_dirty = 1;
        return -1;
      }
      file->filehash_partial = cur->partialhash;
      if (cur->hashcount == 2) {
        file->filehash = cur->fullhash;
        SETFLAG(file->flags, (FF_HASH_PARTIAL | FF_HASH_FULL));
      } else SETFLAG(file->flags, FF_HASH_PARTIAL);
      return 1;
    }
  }
  return 0;

error_null:
  fprintf(stderr, "error: internal error: NULL data passed to read_hashdb_entry()\n");
  return -255;
error_path_hash:
  fprintf(stderr, "error: internal error hashing a path\n");
  return -255;
}
