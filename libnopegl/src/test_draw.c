/*
 * Copyright 2019-2022 GoPro Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "drawutils.h"
#include "utils/utils.h"
#include "utils/crc32.h"

static int save_ppm(const char *filename, uint8_t *data, int32_t width, int32_t height)
{
    int ret = 0;
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Unable to open '%s'\n", filename);
        return -1;
    }

    const size_t img_size = (size_t)(width * height * 3);
    uint8_t *buf = malloc(32 + img_size);
    if (!buf) {
        ret = -1;
        goto end;
    }

    const int header_size = snprintf((char *)buf, 32, "P6 %d %d 255\n", width, height);
    if (header_size < 0) {
        ret = -1;
        fprintf(stderr, "Failed to write PPM header\n");
        goto end;
    }

    uint8_t *dst = buf + header_size;
    for (int i = 0; i < width * height; i++) {
        memcpy(dst, data, 3);
        dst += 3;
        data += 4;
    }

    const size_t size = (size_t)header_size + img_size;
    const size_t n = fwrite(buf, 1, size, fp);
    if (n != size) {
        ret = -1;
        fprintf(stderr, "Failed to write PPM data\n");
        goto end;
    }

end:
    free(buf);
    fclose(fp);
    return ret;
}

static uint32_t get_checkerboard(int x, int y)
{
    return ((x + y) & 1) ? 0x222222ff : 0x444444ff;
}

int main(int ac, char **av)
{
    if (ac != 2) {
        fprintf(stderr, "Usage: %s <output.ppm>\n", av[0]);
        return -1;
    }

    struct canvas c = {.w = 16 * NGLI_FONT_W, .h = 8 * NGLI_FONT_H};
    const size_t buf_size = (size_t)(c.w * c.h);
    c.buf = calloc(buf_size, 4);
    if (!c.buf)
        return -1;

    char s[2] = {0};
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 16; x++) {
            struct rect rect = {
                .x = x * NGLI_FONT_W,
                .y = y * NGLI_FONT_H,
                .w = NGLI_FONT_W,
                .h = NGLI_FONT_H,
            };
            ngli_drawutils_draw_rect(&c, &rect, get_checkerboard(x, y));
            ngli_drawutils_print(&c, rect.x, rect.y, s, 0xffffffff);
            s[0]++;
        }
    }

    uint32_t crc = ngli_crc32_mem(c.buf, buf_size * 4, NGLI_CRC32_INIT);
    printf("CRC: 0x%08x\n", crc);
    ngli_assert(crc == 0x2a07363c);

    int ret = save_ppm(av[1], c.buf, c.w, c.h) < 0 ? -1 : 0;
    free(c.buf);
    return ret;
}
