/* Copyright (C) 2010  John Whitney
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
 * Author: John Whitney <jjw@linuxmail.org>
 */

#include "Python.h"
#define TOKEN_SIZE 2
#define ALLOW_OVERLAP
#include "libbdelta.cpp"
#include "string.h"
void *mem_read(void *data, void *buf, unsigned place, unsigned num) {
	return ((Token*)data) + place;
}

void dumpContent(const char *name, char *s, size_t len) {
	printf("%s: ", name);
	for (int i = 0; i < len; ++i)
		if (isprint(s[i]))
			printf("%c", s[i]);
		else
			printf("%i", s[i]);
	printf("\n\n");
}

PyObject* bdelta_SimpleString(PyObject* self, PyObject* args) {
	Py_UNICODE *a, *b;
	int len_a, len_b;
	int smallestMatch;

	if (!PyArg_ParseTuple(args, "u#u#i", &a, &len_a, &b, &len_b, &smallestMatch))
		return NULL;

	// Find all matches bigger than "smallestMatch" parameter.
	// We achieve this by using a blocksize of "size / 2" (and leaving CARELESSMATCH undefined).
	smallestMatch /= 2;

	PyObject *a16 = PyUnicode_EncodeUTF16(a, len_a, NULL, -1);
	PyObject *b16 = PyUnicode_EncodeUTF16(b, len_b, NULL, -1);
	
#ifndef NDEBUG
	dumpContent("String 1", PyString_AsString(a16), len_a * 2);
	dumpContent("String 2", PyString_AsString(b16), len_b * 2);
#endif
	void *string_a = PyString_AsString(a16);
	void *string_b = PyString_AsString(b16);
	void *bi = bdelta_init_alg(len_a, len_b, mem_read, string_a, string_b, 2);
	int nummatches;
	for (int i = 64; i >= smallestMatch; i /= 2)
		nummatches = bdelta_pass(bi, i);

	PyObject *ret = PyTuple_New(nummatches);
	for (int i = 0; i < nummatches; ++i) {
		unsigned p1, p2, num;
		bdelta_getMatch(bi, i, &p1, &p2, &num);

		PyObject *m = PyTuple_New(3);
		PyTuple_SetItem(m, 0, PyInt_FromLong(p1));
		PyTuple_SetItem(m, 1, PyInt_FromLong(p2));
		PyTuple_SetItem(m, 2, PyInt_FromLong(num));
		PyTuple_SetItem(ret, i, m);
	}
	Py_DECREF(a16);
	Py_DECREF(b16);
	return ret;
}

static PyMethodDef BDeltaMethods[] =
{
     {"bdelta_SimpleString", bdelta_SimpleString, METH_VARARGS, "Get the delta of two strings (returns a list of matches)."},
     {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC

initbdelta_python(void)
{
     (void) Py_InitModule("bdelta_python", BDeltaMethods);
}

