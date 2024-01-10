/* Print comprehensive information to stdout in JSON format
 * This file is part of jdupes; see jdupes.c for license information */

#ifndef NO_JSON

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <libjodycode.h>
#include "likely_unlikely.h"
#include "jdupes.h"
#include "version.h"
#include "act_printjson.h"

/* UTF-8 continuation-bytes are formatted 0b10xx_xxxx */
#define IS_CONT(a)  (((a) & 0xc0) == 0x80)
#define GET_CONT(a) ((a) & 0x3f)
#define TO_HEX(a)   (char)(((a) & 0x0f) <= 0x09 ? ((a) & 0x0f) + 0x30 : ((a) & 0x0f) + 0x57)

/* Amount of space to allocate to be sure escaping is safe. "\v" -> "\u000b" > */
#define JSON_ESCAPE_SIZE(x) ((x) * 6 + 1)

/** Decodes a single UTF-8 codepoint, consuming bytes. */
static inline uint32_t decode_utf8(const char * restrict * const string) {
  uint32_t ret = 0;

  /** ASCII. */
  if (likely(!(**string & 0x80)))
    return (uint32_t)*(*string)++;

  /** UTF-8 2 Byte Sequence */
  if ((**string & 0xe0) == 0xc0) {
    if (likely(IS_CONT((*string)[1]))) {
      ret = *(*string)++ & 0x1f;
      ret = (ret << 6) | GET_CONT(*(*string)++);
      return ret;
    }
  }

  /** UTF-8 3 Byte Sequence */
  if ((**string & 0xf0) == 0xe0) {
    if (likely(IS_CONT((*string)[1]) && IS_CONT((*string)[2]))) {
      ret = *(*string)++ & 0x0f;
      ret = (ret << 6) | GET_CONT(*(*string)++);
      ret = (ret << 6) | GET_CONT(*(*string)++);
      return ret;
    }
  }

  /** UTF-8 4 Byte Sequence */
  if ((**string & 0xf8) == 0xf0) {
    if (likely(IS_CONT((*string)[1]) && IS_CONT((*string)[2]) && IS_CONT((*string)[3]))) {
      ret = *(*string)++ & 0x07;
      ret = (ret << 6) | GET_CONT(*(*string)++);
      ret = (ret << 6) | GET_CONT(*(*string)++);
      ret = (ret << 6) | GET_CONT(*(*string)++);
      return ret;
    }
  }

  /** Encoding error. There is no 5 and 6 multi-bytes.
   * Possibly continuation byte with no preceding multi-byte.
   * Treat as invalid unicode character that needs substitution. */
  ret = (unsigned char) *(*string)++;
  return ret;
}

/** Escapes a single UTF-16 code unit for JSON. */
static inline void escape_uni16(uint16_t u16, char **target) {
  *(*target)++ = '\\';
  *(*target)++ = 'u';
  *(*target)++ = TO_HEX(u16 >> 12);
  *(*target)++ = TO_HEX(u16 >> 8);
  *(*target)++ = TO_HEX(u16 >> 4);
  *(*target)++ = TO_HEX(u16);
}

/** Escapes a UTF-8 string to ASCII JSON format. */
static void json_escape(const char * restrict string, char * restrict target)
{
  uint32_t curr;
  char *escaped = target;

  while (*string != '\0') {
    /* Escape Sequences */
    switch (*string) {
      case  '"': curr =  '"'; break; /* Quotation Mark */
      case '\\': curr = '\\'; break; /* Reverse Slash */
      case '\b': curr =  'b'; break; /* Backspace */
      case '\f': curr =  'f'; break; /* Form Feed */
      case '\n': curr =  'n'; break; /* Line Feed */
      case '\r': curr =  'r'; break; /* Carriage Return */
      case '\t': curr =  't'; break; /* Tab */
      default  : curr = '\0';        /* Other */
    }

    if (curr != '\0') {
      *escaped++ = '\\';
      *escaped++ = (char)curr;
      string++;
    } else {
      curr = decode_utf8(&string);

      if (likely(0x20 <= curr && curr < 0x7f)) {
        /* ASCII Non-Control */
        *escaped++ = (char)curr;
      } else if (curr < 0xffff) {
        /* UTF 16-bit Codepoint */
        escape_uni16((uint16_t)curr, &escaped);
      } else {
        /* UTF 32-bit Codepoint */
        curr -= 0x10000;
        escape_uni16((uint16_t)(0xD800 + ((curr >> 10) & 0x03ff)), &escaped);
        escape_uni16((uint16_t)(0xDC00 + (curr & 0x03ff)), &escaped);
      }
    }
  }
  *escaped = '\0';
}

void printjson(file_t * restrict files, const int argc, char **argv)
{
  file_t * restrict tmpfile;
  int arg = 0, comma = 0;
  char *temp = NULL;

  LOUD(fprintf(stderr, "printjson: %p\n", files));

  /* Output information about the jdupes command environment */
  printf("{\n  \"jdupesVersion\": \"%s\",\n  \"jdupesVersionDate\": \"%s\",\n", VER, VERDATE);

  printf("  \"commandLine\": \"");

  /* Confirm argc is a positive value */
  assert(argc > 0);

  /* Escape argv values */
  while (1) {
    temp = realloc(temp, JSON_ESCAPE_SIZE(strlen(argv[arg])));
    if (temp == NULL) jc_oom("print_json() temp");

    json_escape(argv[arg], temp);
    printf("%s", temp);

    if (++arg < argc) {
      /* Add space as separator */
      printf(" ");
    } else {
      /* End of list */
      printf("\",\n");
      break;
    }
  }

  printf("  \"extensionFlags\": \"");
#ifndef NO_HELPTEXT
  if (feature_flags[0] == NULL) printf("none\",\n");
  else for (int c = 0; feature_flags[c] != NULL; c++)
    printf("%s%s", feature_flags[c], feature_flags[c+1] == NULL ? "\",\n" : " ");
#else
  printf("unavailable\",\n");
#endif

  temp = realloc(temp, JSON_ESCAPE_SIZE(PATHBUF_SIZE));

  printf("  \"matchSets\": [");

  while (files != NULL) {
    if (ISFLAG(files->flags, FF_HAS_DUPES)) {
      if (comma) printf(",");

      printf(
        "\n"
        "    {\n"
        "      \"fileSize\": %" PRIdMAX ",\n"
        "      \"fileList\": [\n"
        "        { \"filePath\": \"",
        (intmax_t)files->size);

      json_escape(files->d_name, temp);
      jc_fwprint(stdout, temp, 0);
      printf("\"");
      tmpfile = files->duplicates;

      while (tmpfile != NULL) {
        printf(" },\n        { \"filePath\": \"");
        json_escape(tmpfile->d_name, temp);
        jc_fwprint(stdout, temp, 0);
        printf("\"");
        tmpfile = tmpfile->duplicates;
      }

      printf(" }\n      ]\n    }");
      comma = 1;
    }
    files = files->next;
  }

  if (comma) {
    printf("\n   ");
  }

  printf("]\n}\n");

  free(temp);
  return;
}

#endif /* NO_JSON */
