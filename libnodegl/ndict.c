/*
 * Copyright 2017 GoPro Inc.
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

#include <stdlib.h>
#include <string.h>

#include "ndict.h"
#include "nodegl.h"

struct ndict {
    struct ndict_entry *entries;
    int nb_entries;
};

int ngli_ndict_count(struct ndict *ndict)
{
    return ndict ? ndict->nb_entries : 0;
}

struct ndict_entry *ngli_ndict_get(struct ndict *ndict, const char *name, struct ndict_entry *prev)
{
    int i = 0;

    if (!ndict)
        return NULL;

    if (prev)
        i = prev - ndict->entries + 1;

    for (; i < ndict->nb_entries; i++) {
        struct ndict_entry *entry = &ndict->entries[i];
        if (!name || !strcmp(name, entry->name))
            return entry;
    }

    return NULL;
}

int ngli_ndict_set(struct ndict **ndictp, const char *name, struct ngl_node *node)
{
    if (!name)
        return -1;

    if (!*ndictp) {
        *ndictp = calloc(1, sizeof(struct ndict));
        if (!*ndictp)
            return -1;
    }

    struct ndict *ndict = *ndictp;

    for (int i = 0; i < ndict->nb_entries; i++) {
        struct ndict_entry *entry = &ndict->entries[i];
        if (!strcmp(name, entry->name)) {
            if (node) {
                ngl_node_unrefp(&entry->node);
                entry->node = ngl_node_ref(node);
            } else {
                memmove(&ndict->entries[i],
                        &ndict->entries[i + 1],
                        (ndict->nb_entries - i - 1) * sizeof(*ndict->entries));
                ndict->nb_entries--;
            }
            return 0;
        }
    }

    if (!node)
        return 0;

    struct ndict_entry *entries = realloc(ndict->entries,
                                          (ndict->nb_entries + 1) * sizeof(*ndict->entries));
    if (!entries)
        return -1;

    struct ndict_entry *entry = &entries[ndict->nb_entries];
    entry->name = ngli_strdup(name);
    if (!entry->name)
        return -1;
    entry->node = ngl_node_ref(node);

    ndict->entries = entries;
    ndict->nb_entries++;

    return 0;
}

void ngli_ndict_freep(struct ndict **ndictp)
{
    if (!*ndictp)
        return;

    struct ndict *ndict = *ndictp;

    for (int i = 0; i < ndict->nb_entries; i++) {
        struct ndict_entry *entry = &ndict->entries[i];
        free(entry->name);
        ngl_node_unrefp(&entry->node);
    }

    free(ndict);
    *ndictp = NULL;
}
