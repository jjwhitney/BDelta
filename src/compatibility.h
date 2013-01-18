/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Fix for MSVC++, which doesn't support Variable Length Arrays.
#ifdef _MSC_VER
	#include <malloc.h>
	#define STACK_ALLOC(name, type, num) type *name = (type *)alloca(sizeof(type) * num)

	typedef unsigned __int8 uint8_t;
	typedef unsigned __int16 uint16_t;
	typedef unsigned __int32 uint32_t;
	typedef unsigned __int64 uint64_t;
#else
	#include <stdint.h>
	#define STACK_ALLOC(name, type, num) type name[num]
#endif
