/* Copyright (C) 2003-2010  John Whitney
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author: John Whitney <jjw@deltup.org>
 */

void fread_fixed(FILE *f, void *buf, unsigned num_bytes) {
	if (fread(&buf, 1, num_bytes, f) != num_bytes)
		throw "File read error.";
}

void fwrite_fixed(FILE *f, const void *buf, unsigned num_bytes) {
	if (fwrite(&buf, 1, num_bytes, f) != num_bytes)
		throw "File write error.";
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
