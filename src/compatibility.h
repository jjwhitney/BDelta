/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Fix for MSVC++, which doesn't support Variable Length Arrays.
#include <inttypes.h>
#include <malloc.h>

#ifdef _MSC_VER
    #define STACK_ALLOC(name, type, num) type *name = (type *)alloca(sizeof(type) * num)
#else
    #define STACK_ALLOC(name, type, num) type name[num]
#endif
