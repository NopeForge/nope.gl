/*
 * Copyright 2020 GoPro Inc.
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


#ifndef OPTS_H
#define OPTS_H

#include <stddef.h>

enum opt_type {
    OPT_TYPE_UNKNOWN = -1,
    OPT_TYPE_TOGGLE,
    OPT_TYPE_INT,
    OPT_TYPE_STR,
    OPT_TYPE_TIME,
    OPT_TYPE_LOGLEVEL,
    OPT_TYPE_BACKEND,
    OPT_TYPE_RATIONAL,
    OPT_TYPE_COLOR,
    OPT_TYPE_CUSTOM,
    OPT_TYPE_NB
};

struct opt {
    const char *short_name;
    const char *name;
    enum opt_type type;
    size_t offset;
    int (*func)(const char *arg, void *dst);
};

#define OPT_HELP -101

int opts_parse(int ac, int ac_max, char **av, const struct opt *opts, int nb_opts, void *dst);
void opts_print_usage(const char *program, const struct opt *opts, int nb_opts, const char *usage_extra);

#endif
