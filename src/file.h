/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

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
