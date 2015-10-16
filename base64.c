#include "base64.h"

#include <assert.h>
#include <string.h>

#include "loop.h"

static char const* const value_to_char =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789"
"+/";

/* this array is generated by
   gen_base64_reverse() below */
static unsigned char const char_to_value[256] = {
255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,
255,255,255, 62,255,255,255, 63,
 52, 53, 54, 55, 56, 57, 58, 59,
 60, 61,255,255,255,  0,255,255,

255,  0,  1,  2,  3,  4,  5,  6,
  7,  8,  9, 10, 11, 12, 13, 14,
 15, 16, 17, 18, 19, 20, 21, 22,
 23, 24, 25,255,255,255,255,255,
255, 26, 27, 28, 29, 30, 31, 32,
 33, 34, 35, 36, 37, 38, 39, 40,
 41, 42, 43, 44, 45, 46, 47, 48,
 49, 50, 51,255,255,255,255,255,

255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,

255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255
};

static void encode_3(unsigned char const* in, char* out)
{
  out[0] = value_to_char[in[0] >> 2];
  out[1] = value_to_char[((in[0] << 4) & 0x30) | (in[1] >> 4)];
  out[2] = value_to_char[((in[1] << 2) & 0x3C) | (in[2] >> 6)];
  out[3] = value_to_char[in[2] & 0x3F];
}

static void encode_2(unsigned char const* in, char* out)
{
  out[0] = value_to_char[in[0] >> 2];
  out[1] = value_to_char[((in[0] << 4) & 0x30) | (in[1] >> 4)];
  out[2] = value_to_char[(in[1] << 2) & 0x3C];
  out[3] = '=';
}

static void encode_1(unsigned char const* in, char* out)
{
  out[0] = value_to_char[in[0] >> 2];
  out[1] = value_to_char[(in[0] << 4) & 0x30];
  out[2] = '=';
  out[3] = '=';
}

static void decode_4(char const* in, unsigned char* out)
{
  unsigned char val[4];
  for (unsigned i = 0; i < 4; ++i) {
    val[i] = char_to_value[(unsigned) in[i]];
    assert(val[i] < 64);
  }
  out[0] = ((val[0] << 2) & 0xFC) | ((val[1] >> 4) & 0x03);
  out[1] = ((val[1] << 4) & 0xF0) | ((val[2] >> 2) & 0x0F);
  out[2] = ((val[2] << 6) & 0xC0) | ((val[3] >> 0) & 0x3F);
}

char* base64_encode(void const* data, unsigned long size)
{
  unsigned long quot = size / 3;
  unsigned long rem = size % 3;
  unsigned long nunits = rem ? (quot + 1) : quot;
  unsigned long nchars = nunits * 4 + 1;
  char* out = LOOP_HOST_MALLOC(char, nchars);
  unsigned char const* in = (unsigned char const*) data;
  for (unsigned long i = 0; i < quot; ++i)
    encode_3(in + i * 3, out + i * 4);
  switch (rem) {
    case 0: break;
    case 1: encode_1(in + quot * 3, out + quot * 4); break;
    case 2: encode_2(in + quot * 3, out + quot * 4); break;
  }
  out[nchars - 1] = '\0';
  return out;
}

void* base64_decode(char const** text, unsigned long* size)
{
  unsigned long nchars = strlen(*text);
  assert(nchars % 4 == 0);
  unsigned long nunits = nchars / 4;
  *size = nunits * 3;
  unsigned char* out = LOOP_HOST_MALLOC(unsigned char, *size);
  char const* in = *text;
  for (unsigned long i = 0; i < nunits; ++i)
    decode_4(in + i * 4, out + i * 3);
  for (unsigned i = 0; i < 2; ++i) {
    if (in[nchars - i - 1] != '=')
      break;
    --(*size);
  }
  *text = in + nunits * 4;
  return out;
}

char* base64_fread(FILE* f, unsigned long* nchars)
{
  *nchars = 0;
  while (1) {
    int c = fgetc(f);
    if (c < 0 || c > 127)
      break;
    unsigned char val = char_to_value[c];
    if (val > 63)
      break;
    ++(*nchars);
  }
  char* out = LOOP_HOST_MALLOC(char, *nchars);
  fseek(f, -((long) *nchars), SEEK_CUR);
  fread(out, 1, *nchars, f);
  return out;
}

void print_base64_reverse(void)
{
  unsigned char* a = LOOP_HOST_MALLOC(unsigned char, 256);
  for (unsigned i = 0; i < 256; ++i)
    a[i] = 255;
  for (unsigned i = 0; i < 64; ++i)
    a[(unsigned) value_to_char[i]] = (unsigned char) i;
  a['='] = 0;
  for (unsigned i = 0; i < 256; ++i) {
    printf("%3u,", (unsigned) a[i]);
    if (i % 8 == 7)
      printf("\n");
    if (i % 64 == 63)
      printf("\n");
  }
  loop_host_free(a);
}
