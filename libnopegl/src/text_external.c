/*
 * Copyright 2023 Nope Forge
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

#include "config.h"

#if HAVE_TEXT_LIBRARIES
#include <hb.h>
#include <hb-ft.h>
#include <ft2build.h>
#include FT_OUTLINE_H
#include <fribidi.h>
#endif

#include "distmap.h"
#include "internal.h"
#include "log.h"
#include "node_text.h"
#include "nopegl.h"
#include "path.h"
#include "utils/darray.h"
#include "utils/hmap.h"
#include "utils/memory.h"
#include "text.h"
#include "utils/utils.h"

#if HAVE_TEXT_LIBRARIES
struct text_external {
    struct darray ft_faces; // FT_Face (hidden pointer)
    struct darray hb_fonts; // hb_font_t*
    struct distmap *distmap;
};

static int load_font(struct text *text, const char *font_file, int32_t face_index)
{
    struct text_external *s = text->priv_data;

    /* This limitation simplifies the UID computation in GLYPH_UID_STRING() */
    if (ngli_darray_count(&s->ft_faces) == 0xff) {
        LOG(ERROR, "maximum number of fonts reached (256)");
        return NGL_ERROR_LIMIT_EXCEEDED;
    }

    FT_Face ft_face = NULL;
    hb_font_t *hb_font = NULL;

    FT_Error ft_error = FT_New_Face(text->ctx->ft_library, font_file, face_index, &ft_face);
    if (ft_error) {
        LOG(ERROR, "unable to initialize FreeType with font %s face %d", font_file, face_index);
        return NGL_ERROR_EXTERNAL;
    }

    if (!FT_IS_SCALABLE(ft_face)) {
        LOG(ERROR, "only scalable faces are supported");
        return NGL_ERROR_UNSUPPORTED;
    }

    if (!ngli_darray_push(&s->ft_faces, &ft_face)) {
        FT_Done_Face(ft_face);
        return NGL_ERROR_MEMORY;
    }

    const int32_t pt_size = text->config.pt_size;
    const FT_F26Dot6 chr_w = NGLI_I32_TO_I26D6(pt_size); // nominal width in 26.6
    const FT_F26Dot6 chr_h = NGLI_I32_TO_I26D6(pt_size); // nominal height in 26.6
    const FT_UInt res = (FT_UInt)text->config.dpi; // resolution in dpi
    ft_error = FT_Set_Char_Size(ft_face, chr_w, chr_h, res, res);
    if (ft_error) {
        LOG(ERROR, "unable to set char size to %d points in %u DPI", pt_size, res);
        return NGL_ERROR_EXTERNAL;
    }

    LOG(DEBUG, "loaded font family %s", ft_face->family_name);
    if (ft_face->style_name)
        LOG(DEBUG, "* style: %s", ft_face->style_name);
    LOG(DEBUG, "* num glyphs: %ld", ft_face->num_glyphs);
    LOG(DEBUG, "* bbox xmin:%ld xmax:%ld ymin:%ld ymax:%ld",
        ft_face->bbox.xMin, ft_face->bbox.xMax,
        ft_face->bbox.yMin, ft_face->bbox.yMax);
    LOG(DEBUG, "* units_per_EM: %d ", ft_face->units_per_EM);
    LOG(DEBUG, "* ascender:  %d ", ft_face->ascender);
    LOG(DEBUG, "* descender: %d ", ft_face->descender);
    LOG(DEBUG, "* height: %d ", ft_face->height);
    LOG(DEBUG, "* max_advance_[width:%d height:%d]",
        ft_face->max_advance_width, ft_face->max_advance_height);
    LOG(DEBUG, "* underline_[position:%d thickness:%d]",
        ft_face->underline_position, ft_face->underline_thickness);

    hb_font = hb_ft_font_create(ft_face, NULL);
    if (!hb_font)
        return NGL_ERROR_MEMORY;

    if (!ngli_darray_push(&s->hb_fonts, &hb_font)) {
        hb_font_destroy(hb_font);
        return NGL_ERROR_MEMORY;
    }

    return 0;
}

static void free_ft_face(void *user_arg, void *data)
{
    FT_Face *face = data;
    FT_Done_Face(*face);
}

static void free_hb_font(void *user_arg, void *data)
{
    struct hb_font_t **fontp = data;
    hb_font_destroy(*fontp);
}

static int text_external_init(struct text *text)
{
    struct text_external *s = text->priv_data;

    ngli_darray_init(&s->ft_faces, sizeof(FT_Face), 0);
    ngli_darray_init(&s->hb_fonts, sizeof(hb_font_t *), 0);

    ngli_darray_set_free_func(&s->ft_faces, free_ft_face, NULL);
    ngli_darray_set_free_func(&s->hb_fonts, free_hb_font, NULL);

    for (size_t i = 0; i < text->config.nb_font_faces; i++) {
        const struct ngl_node *face_node = text->config.font_faces[i];
        const struct fontface_opts *face_opts = face_node->opts;
        int ret = load_font(text, face_opts->path, face_opts->index);
        if (ret < 0)
            return ret;
    }

    return 0;
}

struct glyph {
    uint32_t shape_id; // index in the distmap texture
    int32_t width, height; // in 26.6
    int32_t bearing_x, bearing_y; // in 26.6
};

static void free_glyph(void *user_arg, void *data)
{
    struct glyph *glyph = data;
    ngli_freep(&glyph);
}

static struct glyph *create_glyph(void)
{
    struct glyph *glyph = ngli_calloc(1, sizeof(*glyph));
    return glyph;
}

static const char *hex = "0123456789abcdef";

/* Compute a unique glyph identifier string using the face and glyph IDs */
#define GLYPH_UID_STRING(fid, gid) {  \
    hex[(fid) >> 4U & 0xf],     \
    hex[(fid)       & 0xf],     \
    '-',                        \
    hex[(gid) >> 28U & 0xf],    \
    hex[(gid) >> 24U & 0xf],    \
    hex[(gid) >> 20U & 0xf],    \
    hex[(gid) >> 16U & 0xf],    \
    hex[(gid) >> 12U & 0xf],    \
    hex[(gid) >>  8U & 0xf],    \
    hex[(gid) >>  4U & 0xf],    \
    hex[(gid)        & 0xf],    \
    '\0'                        \
}

enum run_type {
    RUN_TYPE_WORD,
    RUN_TYPE_WORDSEP,
    RUN_TYPE_LINEBREAK,
};

struct text_run {
    enum run_type type;
    size_t face_id;
    hb_buffer_t *buffer;
    const hb_glyph_info_t *glyph_infos;
    const hb_glyph_position_t *glyph_positions;
};

struct outline_ctx {
    struct path *path;
    FT_BBox cbox; // current glyph control box
};

struct vec3 { float x, y, z; };

static struct vec3 vec3_from_ftvec2(const struct outline_ctx *s, const FT_Vector *v)
{
    const struct vec3 ret = {
        .x = NGLI_I26D6_TO_F32(v->x - s->cbox.xMin),
        .y = NGLI_I26D6_TO_F32(v->y - s->cbox.yMin),
    };
    return ret;
}

static int move_to_cb(const FT_Vector *ftvec_to, void *user)
{
    const struct outline_ctx *s = user;
    const struct vec3 vec_to = vec3_from_ftvec2(s, ftvec_to);
    const float to[3] = {vec_to.x, vec_to.y, vec_to.z};
    return ngli_path_move_to(s->path, to);
}

static int line_to_cb(const FT_Vector *ftvec_to, void *user)
{
    const struct outline_ctx *s = user;
    const struct vec3 vec_to = vec3_from_ftvec2(s, ftvec_to);
    const float to[3] = {vec_to.x, vec_to.y, vec_to.z};
    return ngli_path_line_to(s->path, to);
}

static int conic_to_cb(const FT_Vector *ftvec_ctl, const FT_Vector *ftvec_to, void *user)
{
    const struct outline_ctx  *s = user;
    const struct vec3 vec_ctl = vec3_from_ftvec2(s, ftvec_ctl);
    const struct vec3 vec_to  = vec3_from_ftvec2(s, ftvec_to);
    const float ctl[3] = {vec_ctl.x, vec_ctl.y, vec_ctl.z};
    const float to[3]  = {vec_to.x,  vec_to.y,  vec_to.z};
    return ngli_path_bezier2_to(s->path, ctl, to);
}

static int cubic_to_cb(const FT_Vector *ftvec_ctl1, const FT_Vector *ftvec_ctl2,
                       const FT_Vector *ftvec_to, void *user)
{
    const struct outline_ctx *s = user;
    const struct vec3 vec_ctl1 = vec3_from_ftvec2(s, ftvec_ctl1);
    const struct vec3 vec_ctl2 = vec3_from_ftvec2(s, ftvec_ctl2);
    const struct vec3 vec_to   = vec3_from_ftvec2(s, ftvec_to);
    const float ctl1[3] = {vec_ctl1.x, vec_ctl1.y, vec_ctl1.z};
    const float ctl2[3] = {vec_ctl2.x, vec_ctl2.y, vec_ctl2.z};
    const float to[3]   = {vec_to.x,   vec_to.y,   vec_to.z};
    return ngli_path_bezier3_to(s->path, ctl1, ctl2, to);
}

static const FT_Outline_Funcs outline_funcs = {
    .move_to  = move_to_cb,
    .line_to  = line_to_cb,
    .conic_to = conic_to_cb,
    .cubic_to = cubic_to_cb,
};

static int build_glyph_index(struct text *text, struct hmap *glyph_index, const struct darray *runs_array)
{
    int ret = 0;
    struct text_external *s = text->priv_data;

    struct path *path = ngli_path_create();
    if (!path)
        return NGL_ERROR_MEMORY;

    const struct text_run *runs = ngli_darray_data(runs_array);
    for (size_t i = 0; i < ngli_darray_count(runs_array); i++) {
        const struct text_run *run = &runs[i];
        if (run->face_id == SIZE_MAX)
            continue;

        const FT_Face *ft_faces = ngli_darray_data(&s->ft_faces);
        const FT_Face ft_face = ft_faces[run->face_id];
        const size_t nb_glyphs = hb_buffer_get_length(run->buffer);
        const hb_glyph_info_t *glyph_infos = run->glyph_infos;

        for (size_t j = 0; j < nb_glyphs; j++) {
            /*
             * We can't use hb_font_get_glyph_name() since the result is not
             * unique. With some font, it may return an empty string for all
             * the glyphs (see ttf-hanazono 20170904 for an example of this).
             */
            const hb_codepoint_t glyph_id = glyph_infos[j].codepoint;
            const char glyph_uid[] = GLYPH_UID_STRING(run->face_id, glyph_id);
            if (ngli_hmap_get_str(glyph_index, glyph_uid))
                continue;

            /*
             * Harfbuzz seems to use NO_HINTING as well, so we may want to stay
             * aligned with it.
             */
            FT_Error ft_error = FT_Load_Glyph(ft_face, glyph_id, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
            if (ft_error) {
                /*
                 * We do not use the "U+XXXX" notation in the format string
                 * because it does not necessarily correspond to the Unicode
                 * codepoint (we are post-shaping so this is a font specific
                 * character code).
                 */
                LOG(ERROR, "unable to load glyph id %u", glyph_id);
                ret = NGL_ERROR_EXTERNAL;
                goto end;
            }

            const FT_GlyphSlot slot = ft_face->glyph;

            ngli_path_clear(path);

            FT_BBox cbox;
            FT_Outline_Get_CBox(&slot->outline, &cbox);

            const struct outline_ctx ft_ctx = {.path=path,.cbox=cbox};
            FT_Outline_Decompose(&slot->outline, &outline_funcs, (void *)&ft_ctx);

            ret = ngli_path_finalize(path);
            if (ret < 0)
                goto end;

            const int32_t shape_w_26d6 = (int32_t)(cbox.xMax - cbox.xMin);
            const int32_t shape_h_26d6 = (int32_t)(cbox.yMax - cbox.yMin);
            const int32_t shape_w = NGLI_I26D6_TO_I32_TRUNCATED(shape_w_26d6);
            const int32_t shape_h = NGLI_I26D6_TO_I32_TRUNCATED(shape_h_26d6);

            // An empty space glyph doesn't need to be rasterized
            if (shape_w <= 0 || shape_h <= 0)
                continue;

            uint32_t shape_id;
            ret = ngli_distmap_add_shape(s->distmap, (uint32_t)shape_w, (uint32_t)shape_h, path, NGLI_DISTMAP_FLAG_PATH_AUTO_CLOSE, &shape_id);
            if (ret < 0)
                goto end;

            /* Save the rasterized glyph in the index */
            struct glyph *glyph = create_glyph();
            if (!glyph) {
                ret = NGL_ERROR_MEMORY;
                goto end;
            }

            glyph->shape_id  = shape_id;
            glyph->width     = shape_w_26d6;
            glyph->height    = shape_h_26d6;
            glyph->bearing_x = (int32_t)ft_ctx.cbox.xMin;
            glyph->bearing_y = (int32_t)ft_ctx.cbox.yMin;

            ret = ngli_hmap_set_str(glyph_index, glyph_uid, glyph);
            if (ret < 0) {
                free_glyph(NULL, glyph);
                goto end;
            }
        }
    }

end:
    ngli_path_freep(&path);
    return ret;
}

static void reset_runs(struct darray *runs_array)
{
    struct text_run *runs = ngli_darray_data(runs_array);
    for (size_t i = 0; i < ngli_darray_count(runs_array); i++)
        hb_buffer_destroy(runs[i].buffer);
    ngli_darray_reset(runs_array);
}

/* Make sure we can hold the whole Unicode codepoints */
NGLI_STATIC_ASSERT(fribidi_chars_are_32bit, sizeof(FriBidiChar) == 4);

static int char_is_linebreak(FriBidiChar ch)
{
    /* Source: https://en.wikipedia.org/wiki/Whitespace_character */
    switch (ch) {
    case 0x000A: /* line feed */
    case 0x000B: /* line tabulation */
    case 0x000C: /* form feed */
    case 0x000D: /* carriage return */
    case 0x0085: /* next line */
    case 0x2028: /* line separator */
    case 0x2029: /* paragraph separator */
        return 1;
    }
    return 0;
}

static int char_is_whitespace(FriBidiChar ch)
{
    return fribidi_get_bidi_type(ch) == FRIBIDI_TYPE_WS;
}

static size_t find_line_end(const FriBidiChar *str, size_t len, size_t start)
{
    for (size_t i = start; i < len; i++)
        if (char_is_linebreak(str[i]))
            return i;
    return len;
}

static int append_run(struct text *text, struct darray *runs_array,
                      const FriBidiChar *str, size_t len, size_t face_id,
                      enum run_type type, size_t start, size_t end)
{
    hb_buffer_t *buffer = hb_buffer_create();
    if (!hb_buffer_allocation_successful(buffer))
        return NGL_ERROR_MEMORY;

    hb_buffer_add_codepoints(buffer, str, (int)len, (unsigned)start, (int)(end - start));

    if (text->config.writing_mode == NGLI_TEXT_WRITING_MODE_VERTICAL_LR ||
        text->config.writing_mode == NGLI_TEXT_WRITING_MODE_VERTICAL_RL) {
        hb_buffer_set_direction(buffer, HB_DIRECTION_TTB);
    } else if (text->config.writing_mode == NGLI_TEXT_WRITING_MODE_HORIZONTAL_TB) {
        /* FriBidi changes the codepoints order from right-to-left where appropriate */
        hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
    } else {
        ngli_assert(0);
    }

    // TODO: expose to the user? The precise knowledge of the language and/or
    // script may affect the choice of symbols, rules and conventions used
    // during shaping.
    // hb_buffer_set_script(buffer, script);
    // hb_buffer_set_language(buffer, hb_language_from_string("en", -1));

    /* Guess direction, script and language (if not previously set) */
    hb_buffer_guess_segment_properties(buffer);

    const struct text_run run = {.type=type, .face_id=face_id, .buffer=buffer};
    if (!ngli_darray_push(runs_array, &run)) {
        hb_buffer_reset(buffer);
        return NGL_ERROR_MEMORY;
    }

    return 0;
}

static size_t find_face_with_codepoint(const struct darray *ft_faces_array, FriBidiChar ch, size_t prev_face_id)
{
    const FT_ULong charcode = (FT_ULong)ch;
    const FT_Face *ft_faces = ngli_darray_data(ft_faces_array);
    for (size_t face_id = 0; face_id < ngli_darray_count(ft_faces_array); face_id++)
        if (FT_Get_Char_Index(ft_faces[face_id], charcode))
            return face_id;
    return SIZE_MAX;
}

/*
 * Split the sequence of codepoints into multiple runs (or just one if there is
 * only one font face needed)
 */
static int split_into_runs(struct text *text, struct darray *runs_array,
                           const FriBidiChar *str, size_t len,
                           enum run_type type, size_t start, size_t end)
{
    struct text_external *s = text->priv_data;

    ngli_assert(start < end);

    size_t prev_face_id = find_face_with_codepoint(&s->ft_faces, str[start], 0);
    size_t pos = start;

    for (;;) {
        size_t face_id;
        do {
            face_id = find_face_with_codepoint(&s->ft_faces, str[pos], prev_face_id);
            if (face_id != prev_face_id)
                break;
            pos++;
        } while (pos < end);

        int ret = append_run(text, runs_array, str, len, prev_face_id, type, start, pos);
        if (ret < 0)
            return ret;

        if (pos == end)
            break;

        start = pos;
        prev_face_id = face_id;
    }

    return 0;
}

static int handle_words_and_wordseps(struct text *text, struct darray *runs_array, const FriBidiChar *str, size_t len)
{
    size_t pos = 0;

    for (;;) {
        /* Handle word separators (spaces) */
        size_t end = pos;
        while (end < len && char_is_whitespace(str[end]))
            end++;
        if (end > pos) {
            int ret = split_into_runs(text, runs_array, str, len, RUN_TYPE_WORDSEP, pos, end);
            if (ret < 0)
                return ret;
        }
        if (end == len)
            break;
        pos = end;

        /* Hande words */
        while (end < len && !char_is_whitespace(str[end]))
            end++;
        if (end > pos) {
            int ret = split_into_runs(text, runs_array, str, len, RUN_TYPE_WORD, pos, end);
            if (ret < 0)
                return ret;
        }
        if (end == len)
            break;
        pos = end;
    }

    return 0;
}

static int handle_line_breaks(struct text *text, struct darray *runs_array,
                              const FriBidiChar *str, size_t len, size_t *pos)
{
    size_t end = *pos;
    while (end < len && char_is_linebreak(str[end]))
        end++;
    if (end > *pos) {
        int ret = split_into_runs(text, runs_array, str, len, RUN_TYPE_LINEBREAK, *pos, end);
        if (ret < 0)
            return ret;
        *pos = end;
    }
    return 0;
}

#define STACK_LIST_SIZE 128

/*
 * We cannot use fribidi_log2vis() because it includes a clumsy shaping causing
 * the following bug: https://github.com/fribidi/fribidi/issues/200
 * This function is pretty much identical without arabic shaping and a few
 * simplifications due to various unused arguments.
 */
static int log2vis(const FriBidiChar *str, int len, FriBidiParType *pbase_dir, FriBidiChar *out_str)
{
    int ret = 0;
    FriBidiCharType bidi_types_stack[STACK_LIST_SIZE];
    FriBidiBracketType bracket_types_stack[STACK_LIST_SIZE];
    FriBidiLevel embedding_levels_stack[STACK_LIST_SIZE];

    const int use_local = len <= STACK_LIST_SIZE;

    FriBidiCharType *bidi_types       = use_local ? bidi_types_stack       : ngli_malloc((size_t)len * sizeof(*bidi_types));
    FriBidiBracketType *bracket_types = use_local ? bracket_types_stack    : ngli_malloc((size_t)len * sizeof(*bracket_types));
    FriBidiLevel *embedding_levels    = use_local ? embedding_levels_stack : ngli_malloc((size_t)len * sizeof(*embedding_levels));
    if (!bidi_types || !bracket_types || !embedding_levels) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }

    fribidi_get_bidi_types(str, len, bidi_types);
    fribidi_get_bracket_types(str, len, bidi_types, bracket_types);
    if (!fribidi_get_par_embedding_levels_ex(bidi_types, bracket_types, len, pbase_dir, embedding_levels)) {
        ret = NGL_ERROR_EXTERNAL;
        goto end;
    }

    memcpy(out_str, str, (size_t)len * sizeof(*str));
    if (!fribidi_reorder_line(FRIBIDI_FLAGS_DEFAULT, bidi_types, len, 0, *pbase_dir, embedding_levels, out_str, NULL)) {
        ret = NGL_ERROR_EXTERNAL;
        goto end;
    }

end:
    if (!use_local) {
        ngli_freep(&bidi_types);
        ngli_freep(&bracket_types);
        ngli_freep(&embedding_levels);
    }
    return ret;
}

/*
 * Split text into runs, where each run is essentially a harfbuzz buffer
 */
static int build_text_runs(struct text *text, const char *str_orig, struct darray *runs_array)
{
    int ret = 0;
    struct text_external *s = text->priv_data;

    const size_t full_len = strlen(str_orig);
    if (full_len > INT32_MAX)
        return NGL_ERROR_LIMIT_EXCEEDED;

    /* Convert the full string in UTF-8 to Unicode codepoints */
    FriBidiChar *codepoints = ngli_calloc(full_len, sizeof(*codepoints));
    if (!codepoints)
        return NGL_ERROR_MEMORY;
    FriBidiStrIndex unicode_len = fribidi_charset_to_unicode(FRIBIDI_CHAR_SET_UTF8, str_orig, (FriBidiStrIndex)full_len, codepoints);
    ngli_assert(unicode_len <= full_len);

    /* Split input into lines and re-order codepoints of each line for bidirectionnal */
    FriBidiParType pbase_dir = FRIBIDI_PAR_ON;
    size_t pos = 0;
    for (;;) {
        ret = handle_line_breaks(text, runs_array, codepoints, (size_t)unicode_len, &pos);
        if (ret < 0)
            goto end;
        if (pos == unicode_len)
            break;

        /*
         * FriBidi works with lines (or paragraphs) so we must break the input
         * into multiple chunks: this is our first level of segmentation.
         */
        size_t end = find_line_end(codepoints, (size_t)unicode_len, pos);
        if (end == pos) {
            ngli_assert(end == unicode_len);
            break;
        }
        ngli_assert(end > pos);
        const size_t len = end - pos;

        /* Transform codepoints array from logical to visual order */
        FriBidiChar *visual_str = ngli_calloc(len, sizeof(*visual_str));
        if (!visual_str) {
            ret = NGL_ERROR_MEMORY;
            goto end;
        }
        ret = log2vis(&codepoints[pos], (int)len, &pbase_dir, visual_str);
        if (ret < 0) {
            ngli_freep(&visual_str);
            goto end;
        }

        /*
         * Each paragraph needs to be split into words: after shaping, it
         * won't be possible to identify whether a character is a glyph, space,
         * linebreak, etc. This our 2nd level of segmentation.
         */
        ret = handle_words_and_wordseps(text, runs_array, visual_str, len);
        ngli_freep(&visual_str);
        if (ret < 0)
            goto end;

        pos += len;
    }

    /* Run shaping on all run buffers */
    struct text_run *runs = ngli_darray_data(runs_array);
    struct hb_font_t **hb_fonts = ngli_darray_data(&s->hb_fonts);
    for (size_t i = 0; i < ngli_darray_count(runs_array); i++) {
        struct text_run *run = &runs[i];
        hb_buffer_t *buffer = run->buffer;
        const size_t face_id = run->face_id != SIZE_MAX ? run->face_id : 0;
        hb_shape(hb_fonts[face_id], buffer, NULL, 0);

        /*
         * Save these pointers because they take a mutable buffer and we want to
         * make sure the output is always the same.
         */
        run->glyph_infos = hb_buffer_get_glyph_infos(buffer, NULL);
        run->glyph_positions = hb_buffer_get_glyph_positions(buffer, NULL);
    }

end:
    ngli_freep(&codepoints);
    return ret;
}

// XXX is this a reasonable thing to use for vertical text as well?
// See also the also the max_advance field
// The value is using 26.6 encoding
#define GET_LINE_ADVANCE(face_id) ((int32_t)(ft_faces[face_id]->size->metrics.height))

static int register_chars(struct text *text, const char *str, struct darray *chars_dst,
                          const struct darray *runs_array, const struct hmap *glyph_index)
{
    struct text_external *s = text->priv_data;

    const int32_t adv_sign = text->config.writing_mode == NGLI_TEXT_WRITING_MODE_VERTICAL_LR ? 1 : -1;

    ngli_assert(ngli_darray_count(&s->ft_faces) > 0);
    const FT_Face *ft_faces = ngli_darray_data(&s->ft_faces);

    hb_position_t x_cur = 0, y_cur = 0;
    int32_t line_advance = GET_LINE_ADVANCE(0);

    const struct text_run *runs = ngli_darray_data(runs_array);
    for (size_t i = 0; i < ngli_darray_count(runs_array); i++) {
        const struct text_run *run = &runs[i];
        const size_t len = hb_buffer_get_length(run->buffer);
        const hb_direction_t direction = hb_buffer_get_direction(run->buffer);

        /* Update line advance in case there was a font change in the middle of the line */
        if (run->face_id != SIZE_MAX) {
            const int32_t run_line_advance = GET_LINE_ADVANCE(run->face_id);
            line_advance = NGLI_MAX(line_advance, run_line_advance);
        }

        for (size_t j = 0; j < len; j++) {
            const hb_glyph_position_t *pos = &run->glyph_positions[j];
            struct char_info_internal chr = {0};

            if (run->type == RUN_TYPE_LINEBREAK) {
                chr.tags = NGLI_TEXT_CHAR_TAG_LINE_BREAK;
                if (HB_DIRECTION_IS_HORIZONTAL(direction)) {
                    x_cur = 0;
                    y_cur += adv_sign * line_advance;
                } else {
                    y_cur = 0;
                    x_cur += adv_sign * line_advance;
                }

                /* Reset line advance to its default value */
                line_advance = GET_LINE_ADVANCE(0);

                /* We assume the linebreak is never displayable */
                if (!ngli_darray_push(chars_dst, &chr))
                    return NGL_ERROR_MEMORY;
                continue;

            } else if (run->type == RUN_TYPE_WORDSEP) {
                chr.tags = NGLI_TEXT_CHAR_TAG_WORD_SEPARATOR;
            }

            const hb_codepoint_t glyph_id = run->glyph_infos[j].codepoint;
            const char glyph_uid[] = GLYPH_UID_STRING(run->face_id, glyph_id);
            const struct glyph *glyph = ngli_hmap_get_str(glyph_index, glyph_uid);
            if (glyph) {
                chr.tags |= NGLI_TEXT_CHAR_TAG_GLYPH;
                chr.x = x_cur + glyph->bearing_x + pos->x_offset;
                chr.y = y_cur + glyph->bearing_y + pos->y_offset;
                chr.w = glyph->width;
                chr.h = glyph->height;
                ngli_distmap_get_shape_coords(s->distmap, glyph->shape_id, chr.atlas_coords);
                ngli_distmap_get_shape_scale(s->distmap, glyph->shape_id, chr.scale);
            }

            if (!ngli_darray_push(chars_dst, &chr))
                return NGL_ERROR_MEMORY;

            x_cur += pos->x_advance;
            y_cur += pos->y_advance;
        }
    }
    return 0;
}

static int text_external_set_string(struct text *text, const char *str, struct darray *chars_dst)
{
    struct text_external *s = text->priv_data;
    struct hmap *glyph_index = NULL;

    struct darray runs_array;
    ngli_darray_init(&runs_array, sizeof(struct text_run), 0);

    /* Re-entrance reset */
    ngli_distmap_freep(&s->distmap);

    int ret = build_text_runs(text, str, &runs_array);
    if (ret < 0)
        goto end;

    s->distmap = ngli_distmap_create(text->ctx);
    if (!s->distmap) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }
    ret = ngli_distmap_init(s->distmap);
    if (ret < 0)
        goto end;

    glyph_index = ngli_hmap_create(NGLI_HMAP_TYPE_STR);
    if (!glyph_index) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }
    ngli_hmap_set_free_func(glyph_index, free_glyph, NULL);
    ret = build_glyph_index(text, glyph_index, &runs_array);
    if (ret < 0)
        goto end;

    ret = ngli_distmap_finalize(s->distmap);
    if (ret < 0)
        goto end;
    text->atlas_texture = ngli_distmap_get_texture(s->distmap);

    ret = register_chars(text, str, chars_dst, &runs_array, glyph_index);
    if (ret < 0)
        goto end;

end:
    reset_runs(&runs_array);
    ngli_hmap_freep(&glyph_index);
    return ret;
}

static void text_external_reset(struct text *text)
{
    struct text_external *s = text->priv_data;

    ngli_darray_reset(&s->hb_fonts);
    ngli_darray_reset(&s->ft_faces);
    ngli_distmap_freep(&s->distmap);
}

const struct text_cls ngli_text_external = {
    .priv_size       = sizeof(struct text_external),
    .init            = text_external_init,
    .set_string      = text_external_set_string,
    .reset           = text_external_reset,
    .flags           = NGLI_TEXT_FLAG_MUTABLE_ATLAS,
};

#else

static int text_external_dummy_set_string(struct text *s, const char *str, struct darray *chars_dst)
{
    return NGL_ERROR_BUG;
}

static int text_external_dummy_init(struct text *s)
{
    return NGL_ERROR_BUG;
}

const struct text_cls ngli_text_external = {
    .init       = text_external_dummy_init,
    .set_string = text_external_dummy_set_string,
};

#endif
