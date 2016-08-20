/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <cstdlib>
#include <inttypes.h>

#define MAX_IO_BLOCK_SIZE (1024 * 1024)

void fread_fixed(FILE *f, void * _buf, unsigned num_bytes) {
	char * buf = (char *)_buf;

	while (num_bytes != 0)
	{
		unsigned block_size = num_bytes;
		if (block_size > MAX_IO_BLOCK_SIZE) block_size = MAX_IO_BLOCK_SIZE;

		size_t r = fread(buf, 1, block_size, f);
		if (r != block_size)
		{
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

	while (num_bytes != 0)
	{
		unsigned block_size = num_bytes;
		if (block_size > MAX_IO_BLOCK_SIZE) block_size = MAX_IO_BLOCK_SIZE;

		size_t r = fwrite(buf, 1, block_size, f);
		if (r != block_size)
		{
			static char write_error_message[128];
			sprintf (write_error_message, "write error: fwrite_fixed(num_bytes=%u) != %u", block_size, (unsigned)r);
			throw write_error_message;
		}
		buf       += block_size;
		num_bytes -= block_size;
	}
}

unsigned read_word(FILE *f) {
	unsigned char b, b2;
	fread_fixed(f, &b, 1);
	fread_fixed(f, &b2, 1);
	return (b2 << 8) + b;
}

unsigned read_dword(FILE *f) {
	unsigned low = read_word(f);
	return (read_word(f) << 16) + low;
}

uint64_t scan_varint(const char* in) {
	uint64_t l = 0;
	for (int i = 0; ; ++i) {
		l += (uint64_t)(in[i] & 0x7f) << (i * 7);
		if (!(in[i] & 0x80))
			return l;
	}
	return 0;
}

int64_t scan_pb_type0_sint(const char* in) {
	uint64_t m = scan_varint(in);
	return (-(m&1)) ^ (m>>1);
}

int64_t read_varint(FILE* f) {
	char buf[10];
	for (size_t i = 0; i < sizeof(buf); ++i) {
		fread_fixed(f, buf+i, 1);
		if (!(buf[i] & 0x80))
			return scan_pb_type0_sint(buf);
	}
	throw "parse error: read_varint() failed";
}

void write_word(FILE *f, unsigned number) {
	unsigned char b = number & 255,
	              b2 = number >> 8;
	fwrite_fixed(f, &b, 1);
	fwrite_fixed(f, &b2, 1);
}

void write_dword(FILE *f, unsigned number) {
	write_word(f, number & 65535);
	write_word(f, number >> 16);
}


/* write int in least amount of bytes, return number of bytes */
/* as used in varints from Google protocol buffers */
int fmt_varint(char* dest, uint64_t l) {
	/* high bit says if more bytes are coming, lower 7 bits are payload; little endian */
	int i;
	for (i = 0; ; ++i) {
		unsigned char bits = l & 0x7f;
		l >>= 7;
		if (l)
			dest[i] = bits | 128;
		else {
			dest[i] = bits;
			break;
		}
	}
	return i + 1;
}

int fmt_pb_type0_sint(char* dest, int64_t l) {
	return fmt_varint(dest, (l << 1) ^ (-(l < 0)));
}

void write_varint(FILE* f, int64_t number) {
	char tmp[10];
	fwrite_fixed(f, tmp, fmt_pb_type0_sint(tmp, number));
}

bool fileExists(char *fname) {
	FILE *f = fopen(fname, "rb");
	bool exists = (f != NULL);
	if (exists) fclose(f);
	return exists;
}

unsigned getLenOfFile(char *fname) {
	FILE *f = fopen(fname, "rb");
	fseek(f, 0, SEEK_END);
	unsigned len = ftell(f);
	fclose(f);
	return len;
}
