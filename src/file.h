/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <cstdlib>
#include "compatibility.h"
#include <sys/stat.h>

#define MAX_IO_BLOCK_SIZE (1024 * 1024)

void fread_fixed(FILE *f, void * _buf, unsigned num_bytes) {
  char * buf = (char *)_buf;

  while (num_bytes != 0) {
    unsigned block_size = num_bytes;
    if (block_size > MAX_IO_BLOCK_SIZE) block_size = MAX_IO_BLOCK_SIZE;
    
    size_t r = fread(buf, 1, block_size, f);
    if (r != block_size) {
      static char read_error_message[128];
      sprintf (read_error_message, "read error: fread_fixed(block_size=%u) != %u", block_size, (unsigned)r);
      throw read_error_message;
    }
    buf       += block_size;
    num_bytes -= block_size;
  }
}

void fwrite_fixed(FILE *f, const void * _buf, unsigned num_bytes) {
  const char * buf = (const char *)_buf;

  while (num_bytes != 0) {
    unsigned block_size = num_bytes;
    if (block_size > MAX_IO_BLOCK_SIZE) block_size = MAX_IO_BLOCK_SIZE;
    
    size_t r = fwrite(buf, 1, block_size, f);
    if (r != block_size) {
      static char write_error_message[128];
      sprintf (write_error_message, "write error: fwrite_fixed(num_bytes=%u) != %u", block_size, (unsigned)r);
      throw write_error_message;
    }
    buf       += block_size;
    num_bytes -= block_size;
  }
}

static size_t scan_varint(const char* in,size_t len, uint64_t* n) {
  size_t i;
  uint64_t l;
  if (len==0) return 0;
  for (l=0, i=0; i<len; ++i) {
    l+=(uint64_t)(in[i]&0x7f) << (i*7);
    if (!(in[i]&0x80)) {
      *n=l;
      return i+1;
    }
  }
  return 0;
}

size_t scan_pb_type0_sint(const char* in,size_t len,int64_t* l) {
  uint64_t m;
  size_t n=scan_varint(in,len,&m);
  if (!n) return 0;
  *l=(-(m&1)) ^ (m>>1);
  return n;
}

int64_t read_varint(FILE* f) {
  char buf[20];
  size_t i;
  int64_t l;
  for (i=0; i<sizeof(buf); ++i) {
    fread_fixed(f,buf+i,1);
    if (!(buf[i]&0x80)) {
      if (scan_pb_type0_sint(buf,i+1,&l)!=i+1) break;
      return l;
    }
  }
  static char read_error_message[128];
  strcpy(read_error_message, "parse error: read_varint() failed");
  throw read_error_message;
}

uint64_t read_varuint(FILE* f) {
  char buf[20];
  size_t i;
  uint64_t l;
  for (i=0; i<sizeof(buf); ++i) {
    fread_fixed(f,buf+i,1);
    if (!(buf[i]&0x80)) {
      if (scan_varint(buf,i+1,&l)!=i+1) break;
      return l;
    }
  }
  static char read_error_message[128];
  strcpy(read_error_message, "parse error: read_varint() failed");
  throw read_error_message;
}

/* write int in least amount of bytes, return number of bytes */
/* as used in varints from Google protocol buffers */
static size_t fmt_varint(char* dest,uint64_t l) {
  /* high bit says if more bytes are coming, lower 7 bits are payload; little endian */
  size_t i;
  for (i=0; l; ++i, l>>=7) {
    if (dest) dest[i]=(l&0x7f) | ((!!(l&~0x7f))<<7);
  }
  if (!i) {	/* l was 0 */
    if (dest) dest[0]=0;
    ++i;
  }
  return i;
}

static size_t fmt_pb_type0_sint(char* dest,int64_t l) {
  return fmt_varint(dest,(l << 1) ^ (l >> (sizeof(l)*8-1)));
}

void write_varint(FILE* f, int64_t number) {
  char tmp[20];
  fwrite_fixed(f,tmp,fmt_pb_type0_sint(tmp,number));
}

void write_varuint(FILE* f, uint64_t number) {
  char tmp[20];
  fwrite_fixed(f,tmp,fmt_varint(tmp,number));
}

bool fileExists(char *fname) {
  FILE *f = fopen(fname, "rb");
  bool exists = (f != NULL);
  if (exists) fclose(f);
  return exists;
}

int64_t getLenOfFile(char *fname) {
  struct stat buf;
  stat(fname, &buf);
  return buf.st_size;
}
