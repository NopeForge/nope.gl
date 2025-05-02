/*
 * Copyright 2016-2022 GoPro Inc.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef UTILS_H
#define UTILS_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "config.h"

#include "nopegl.h"

#ifdef __GNUC__
# ifdef TARGET_MINGW_W64
#  define ngli_printf_format(fmtpos, attrpos) __attribute__((__format__(__MINGW_PRINTF_FORMAT, fmtpos, attrpos)))
# else
#  define ngli_printf_format(fmtpos, attrpos) __attribute__((__format__(__printf__, fmtpos, attrpos)))
# endif
#else
# define ngli_printf_format(fmtpos, attrpos)
#endif

#if defined(__GNUC__) || defined(__clang__)
# define ngli_unused __attribute__((unused))
#else
# define ngli_unused
#endif

#define ngli_assert(cond) do {                          \
    if (!(cond)) {                                      \
        fprintf(stderr, "Assert %s @ %s:%d\n",          \
                #cond, __FILE__, __LINE__);             \
        abort();                                        \
    }                                                   \
} while (0)

#define NGLI_STATIC_ASSERT(id, c) static_assert(c, #id)

#define NGLI_FIELD_SIZEOF(name, field) (sizeof(((name *)0)->field))

#define NGLI_MIN(a, b) ((a) < (b) ? (a) : (b))
#define NGLI_MAX(a, b) ((a) > (b) ? (a) : (b))
#define NGLI_CLAMP(x, min, max) NGLI_MAX(NGLI_MIN(x, max), min)

#if HAVE_BUILTIN_OVERFLOW
#define NGLI_CHK_MUL(result, a, b) __builtin_mul_overflow(a, b, result)
#else
#define NGLI_CHK_MUL(result, a, b) (*(result) = (a) * (b), false)
#endif

#define NGLI_ARRAY_MEMDUP(dst, src, name) do {                             \
    const size_t nb = (src)->nb_##name;                                    \
    if (nb > 0) {                                                          \
        (dst)->name = ngli_memdup((src)->name, nb * sizeof(*(dst)->name)); \
        if (!(dst)->name)                                                  \
            return NGL_ERROR_MEMORY;                                       \
    } else {                                                               \
        (dst)->name = NULL;                                                \
    }                                                                      \
    (dst)->nb_##name = nb;                                                 \
} while (0)                                                                \

#define NGLI_ARRAY_NB(x) (sizeof(x)/sizeof(*(x)))
#define NGLI_SWAP(type, a, b) do { type tmp_swap = b; b = a; a = tmp_swap; } while (0)

#define NGLI_ALIGN_MASK(v, mask) (((v) + (mask)) & ~(mask))
#define NGLI_ALIGN(v, a) NGLI_ALIGN_MASK(v, (__typeof__(v))(a) - 1)
#define NGLI_ALIGN_VAL 16

#define NGLI_ATTR_ALIGNED _Alignas(NGLI_ALIGN_VAL)

#define NGLI_ALIGNED_VEC(vname) float NGLI_ATTR_ALIGNED vname[4]
#define NGLI_ALIGNED_MAT(mname) float NGLI_ATTR_ALIGNED mname[4*4]

#if CONFIG_SMALL
#define NGLI_DOCSTRING(s) (NULL)
#else
#define NGLI_DOCSTRING(s) (s)
#endif

/* Format printf helpers */
#define NGLI_FMT_F    "%12g"
#define NGLI_FMT_VEC2 NGLI_FMT_F " " NGLI_FMT_F
#define NGLI_FMT_VEC3 NGLI_FMT_F " " NGLI_FMT_VEC2
#define NGLI_FMT_VEC4 NGLI_FMT_F " " NGLI_FMT_VEC3

#define NGLI_FMT_MAT2 NGLI_FMT_VEC2 "\n" \
                      NGLI_FMT_VEC2
#define NGLI_FMT_MAT3 NGLI_FMT_VEC3 "\n" \
                      NGLI_FMT_VEC3 "\n" \
                      NGLI_FMT_VEC3
#define NGLI_FMT_MAT4 NGLI_FMT_VEC4 "\n" \
                      NGLI_FMT_VEC4 "\n" \
                      NGLI_FMT_VEC4 "\n" \
                      NGLI_FMT_VEC4

/* Argument helpers associated with formats declared above */
#define NGLI_ARG_F(v)    *(v)
#define NGLI_ARG_VEC2(v) NGLI_ARG_F(v), NGLI_ARG_F((v)+1)
#define NGLI_ARG_VEC3(v) NGLI_ARG_F(v), NGLI_ARG_VEC2((v)+1)
#define NGLI_ARG_VEC4(v) NGLI_ARG_F(v), NGLI_ARG_VEC3((v)+1)

#define NGLI_ARG_MAT2(v) NGLI_ARG_VEC2((v)+2*0),    \
                         NGLI_ARG_VEC2((v)+2*1)
#define NGLI_ARG_MAT3(v) NGLI_ARG_VEC3((v)+3*0),    \
                         NGLI_ARG_VEC3((v)+3*1),    \
                         NGLI_ARG_VEC3((v)+3*2)
#define NGLI_ARG_MAT4(v) NGLI_ARG_VEC4((v)+4*0),    \
                         NGLI_ARG_VEC4((v)+4*1),    \
                         NGLI_ARG_VEC4((v)+4*2),    \
                         NGLI_ARG_VEC4((v)+4*3)

#define NGLI_HAS_ALL_FLAGS(a, b) (((a) & (b)) == (b))

typedef void (*ngli_user_free_func_type)(void *user_arg, void *data);

#endif /* UTILS_H */
