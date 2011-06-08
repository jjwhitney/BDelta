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

// Fix for MSVC++, which doesn't support Variable Length Arrays.
#ifdef _MSC_VER
	#include <malloc.h>
	#define STACK_ALLOC(name, type, num) type *name = (type *)alloca(sizeof(type) * num)
#else
	#define STACK_ALLOC(name, type, num) type name[num]
#endif
