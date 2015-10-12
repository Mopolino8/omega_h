#include "files.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned count_digits(unsigned x)
{
  unsigned l = 0;
  while (x) {
    ++l;
    x /= 10;
  }
  return l;
}

void split_pathname(char const* pathname, char* buf,
    unsigned buf_size, char** filename, char** suffix)
{
  assert(strlen(pathname) < buf_size);
  strcpy(buf, pathname);
  char* dot = strrchr(buf, '.');
  assert(dot);
  *dot = '\0';
  if (suffix)
    *suffix = dot + 1;
  char* slash = strrchr(buf, '/');
  if (slash) {
    *slash = '\0';
    if (filename)
      *filename = slash + 1;
  } else {
    if (filename)
      *filename = buf;
  }
}

void parallel_pathname(char const* prefix, unsigned npieces,
    unsigned piece, char const* suffix, char* buf, unsigned buf_size)
{
  unsigned ndig = count_digits(npieces);
  unsigned long prelen = strlen(prefix);
  unsigned long suflen = strlen(suffix);
  assert(prelen + 1 + ndig + 1 + suflen < buf_size);
  memcpy(buf, prefix, prelen);
  buf += prelen;
  if (npieces > 1) {
    sprintf(buf, "_%0*u", (int) ndig, piece);
    buf += 1 + ndig;
  }
  *buf = '.';
  ++buf;
  memcpy(buf, suffix, suflen);
  buf += suflen;
  *buf = '\0';
}