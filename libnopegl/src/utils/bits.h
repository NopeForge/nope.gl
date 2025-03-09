/*
 * Copyright 2023-2025 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef BITS_H
#define BITS_H

#include <stdint.h>

#ifdef _MSC_VER
#include <intrin.h>
#endif

/*
 * Returns the number of leading 0-bits in x, starting at the most significant
 * bit position. If x is 0, the result is undefined.
 */
static inline uint32_t ngli_clz(uint32_t x)
{
#ifdef _MSC_VER
    unsigned long ret;
    _BitScanReverse(&ret, x);
    return 31 - ret;
#else
    return (uint32_t)__builtin_clz(x);
#endif
}

/*
 * Return the base-2 logarithm of x. If x is 0, the result is undefined.
 */
static inline uint32_t ngli_log2(uint32_t x)
{
    return 31 - ngli_clz(x);
}

#endif /* BITS_H */
