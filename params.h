/*
 * Copyright 2016 Clément Bœsch <cboesch@gopro.com>
 * Copyright 2016 Matthieu Bouron <mbouron@gopro.com>
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

#ifndef PARAMS_H
#define PARAMS_H

#include <stdarg.h>

#include "nodes.h"

int ngli_params_set(uint8_t *base_ptr, const struct node_param *par, va_list *ap);
int ngli_params_set_constructors(uint8_t *base_ptr, const struct node_param *params, va_list *ap);
int ngli_params_set_defaults(uint8_t *base_ptr, const struct node_param *params);
int ngli_params_add(uint8_t *base_ptr, const struct node_param *par, int nb_elems, void *elems);
void ngli_params_free(uint8_t *base_ptr, const struct node_param *params);

#endif
