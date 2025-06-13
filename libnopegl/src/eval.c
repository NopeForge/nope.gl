/*
 * Copyright 2021-2022 GoPro Inc.
 * Copyright 2021-2022 Clément Bœsch <u pkh.me>
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

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "eval.h"
#include "log.h"
#include "math_utils.h"
#include "ngpu/program.h" // for MAX_ID_LEN
#include "nopegl.h"
#include "utils/darray.h"
#include "utils/memory.h"
#include "utils/utils.h"

#define WHITESPACES " \r\n\t"

enum token_type {
    TOKEN_CONSTANT,
    TOKEN_VARIABLE,
    TOKEN_UNARY_OPERATOR,
    TOKEN_BINARY_OPERATOR,
    TOKEN_FUNCTION,
    TOKEN_SPECIAL,
};

static float f_add(float a, float b) { return a + b; }
static float f_sub(float a, float b) { return a - b; }
static float f_mul(float a, float b) { return a * b; }
static float f_div(float a, float b) { return a / b; }

static void *get_binary_operator_func(char c)
{
    switch (c) {
    case '+': return f_add;
    case '-': return f_sub;
    case '*': return f_mul;
    case '/': return f_div;
    }
    ngli_assert(0);
}

static float f_negate(float x) { return -x; }
static float f_noop(float x) { return x; }

static float f_clamp(float x, float min, float max)
{
    return NGLI_CLAMP(x, min, max);
}

static float f_sign(float x)
{
    if (x > 0.f) return 1.f;
    if (x < 0.f) return -1.f;
    return 0.f;
}

static float f_print(float x) { fprintf(stdout, "%g\n", x); return x; }
static float f_fract(float x) { return x - truncf(x); }
static float f_sqr(float x) { return x * x; }
static float f_cube(float x) { return x * x * x; }
static float f_max(float a, float b) { return NGLI_MAX(a, b); }
static float f_min(float a, float b) { return NGLI_MIN(a, b); }
static float f_sat(float x) { return f_clamp(x, 0.f, 1.f); }
static float f_mla(float a, float b, float c) { return a*b + c; }
static float f_mod_e(float a, float b) { return a - b * f_sign(b) * floorf(a / fabsf(b)); }
static float f_mod_f(float a, float b) { return a - b * floorf(a / b); }
static float f_mod_t(float a, float b) { return a - b * truncf(a / b); }
static float f_luma(float r, float g, float b) { return 0.2126f*r + 0.7152f*g + 0.0722f*b; }

static float f_degrees(float x) { return 180.f / PI_F32 * x; }
static float f_radians(float x) { return PI_F32 / 180.f * x; }
static float f_linear2srgb(float x) { return x < 0.0031308f ? x*12.92f : 1.055f * powf(x, 1.f/2.4f)-0.055f; }
static float f_srgb2linear(float x) { return x < 0.04045f   ? x/12.92f : powf((x+.055f) / 1.055f, 2.4f); }
static float f_mix(float a, float b, float x) { return NGLI_MIX_F32(a, b, x); }
static float f_srgbmix(float a, float b, float x) { return f_linear2srgb(f_mix(f_srgb2linear(a), f_srgb2linear(b), x)); }
static float f_linear(float a, float b, float x) { return NGLI_LINEAR_NORM(a, b, x); }
static float f_linearstep(float a, float b, float x) { return f_sat(f_linear(a, b, x)); }

static float f_smooth(float a, float b, float x)
{
    const float t = f_linear(a, b, x);
    return (3.f - 2.f * t) * t * t;
}

static float f_smoothstep(float a, float b, float x)
{
    const float t = f_linearstep(a, b, x);
    return (3.f - 2.f * t) * t * t;
}

/* See https://bitbashing.io/comparing-floats.html */
static float f_close_p(float a, float b, float p) { return (float)(fabsf(a - b) <= p * f_max(fabsf(a), fabsf(b))); }
static float f_close(float a, float b) { return f_close_p(a, b, 1e-6f); }

static float f_eq(float a, float b) { return (float)(a == b); }
static float f_gt(float a, float b) { return (float)(a > b); }
static float f_gte(float a, float b) { return (float)(a >= b); }
static float f_lt(float a, float b) { return (float)(a < b); }
static float f_lte(float a, float b) { return (float)(a <= b); }

static float f_isfinite(float x) { return (float)isfinite(x); }
static float f_isinf(float x) { return (float)isinf(x); }
static float f_isnan(float x) { return (float)isnan(x); }
static float f_isnormal(float x) { return (float)isnormal(x); }

#ifdef _WIN32
/* Make sure all the math functions are declared as such and not as intrinsics so we can get their addresses */
#pragma function(fabsf, acosf, acoshf, asinf, asinhf, atanf, atanhf, cbrtf, ceilf,  \
                 cosf, coshf, erff, expf, exp2f, floorf, hypotf, logf, log2f, powf, \
                 roundf, sinf, sinhf, sqrtf, tanf, tanhf, truncf)
#endif

static const struct function {
    const char *name;
    int nb_args;
    void *func;
} functions_map[] = {
    {"abs",         1, fabsf},
    {"acos",        1, acosf},
    {"acosh",       1, acoshf},
    {"asin",        1, asinf},
    {"asinh",       1, asinhf},
    {"atan",        1, atanf},
    {"atanh",       1, atanhf},
    {"cbrt",        1, cbrtf},
    {"ceil",        1, ceilf},
    {"clamp",       3, f_clamp},
    {"close",       2, f_close},
    {"close_p",     3, f_close_p},
    {"cos",         1, cosf},
    {"cosh",        1, coshf},
    {"cube",        1, f_cube},
    {"degrees",     1, f_degrees},
    {"eq",          2, f_eq},
    {"erf",         1, erff},
    {"exp",         1, expf},
    {"exp2",        1, exp2f},
    {"floor",       1, floorf},
    {"fract",       1, f_fract},
    {"gt",          2, f_gt},
    {"gte",         2, f_gte},
    {"hypot",       2, hypotf},
    {"isfinite",    1, f_isfinite},
    {"isinf",       1, f_isinf},
    {"isnan",       1, f_isnan},
    {"isnormal",    1, f_isnormal},
    {"linear",      3, f_linear},
    {"linear2srgb", 1, f_linear2srgb},
    {"linearstep",  3, f_linearstep},
    {"log",         1, logf},
    {"log2",        1, log2f},
    {"lt",          2, f_lt},
    {"lte",         2, f_lte},
    {"luma",        3, f_luma},
    {"max",         2, f_max},
    {"min",         2, f_min},
    {"mix",         3, f_mix},
    {"mla",         3, f_mla},
    {"mod_e",       2, f_mod_e},
    {"mod_f",       2, f_mod_f},
    {"mod_t",       2, f_mod_t},
    {"pow",         2, powf},
    {"print",       1, f_print},
    {"radians",     1, f_radians},
    {"round",       1, roundf},
    {"sat",         1, f_sat},
    {"sign",        1, f_sign},
    {"sin",         1, sinf},
    {"sinh",        1, sinhf},
    {"smooth",      3, f_smooth},
    {"smoothstep",  3, f_smoothstep},
    {"sqr",         1, f_sqr},
    {"sqrt",        1, sqrtf},
    {"srgb2linear", 1, f_srgb2linear},
    {"srgbmix",     3, f_srgbmix},
    {"tan",         1, tanf},
    {"tanh",        1, tanhf},
    {"trunc",       1, truncf},
};

static const struct constant {
    const char *name;
    float value;
} constants_map[] = {
    {"e",   2.7182818284590451f},
    {"phi", 1.618033988749895f},
    {"pi",  PI_F32},
    {"tau", TAU_F32},
};

struct token {
    enum token_type type;
    int precedence;
    size_t pos;            // position of the token in the input string
    char chr;           // TOKEN_UNARY_OPERATOR, TOKEN_BINARY_OPERATOR, TOKEN_SPECIAL
    float value;        // TOKEN_CONSTANT
    const float *ptr;   // TOKEN_VARIABLE (pointer to the changing data)
    const char *name;   // TOKEN_FUNCTION (pointer to functions_map[].name)
    union {
        void *f;
        float (*f1)(float a);
        float (*f2)(float a, float b);
        float (*f3)(float a, float b, float c);
    } func; // TOKEN_UNARY_OPERATOR, TOKEN_BINARY_OPERATOR, TOKEN_FUNCTION
    int nb_args;
};

struct eval {
    struct darray tokens;       // user input, infix notation
    struct darray tmp_stack;    // temporary token stack
    struct darray output;       // tokens in in RPN
    struct hmap *funcs;         // hash map of functions_map
    struct hmap *consts;        // hash map of constants_map
    const struct hmap *vars;    // hash map of user variables
};

struct eval *ngli_eval_create(void)
{
    struct eval *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    ngli_darray_init(&s->tokens, sizeof(struct token), 0);
    ngli_darray_init(&s->tmp_stack, sizeof(struct token), 0);
    ngli_darray_init(&s->output, sizeof(struct token), 0);
    return s;
}

#define MAX_PRECEDENCE 3

static int get_binary_operator_precedence(char operator)
{
    if (operator == '*' || operator == '/')
        return 2;
    if (operator == '+' || operator == '-')
        return 1;
    return 0;
}

#define PUSH(dst, token) do {               \
    if (!ngli_darray_push(dst, token))      \
        return NGL_ERROR_MEMORY;            \
} while (0)

/*
 * Prototypes for the recursive descent parsing:
 * - parse_subexpr: a sub-expression is basically anything that composes a value
 * - parse_post_subexpr: what follows a sub-expression; an ending paren, a
 *   comma or binary operator separates sub-expressions
 * - parse_opening_paren: special case for what follows a function
 *
 * Each of these parsing functions calls the next one in a chain (through tail
 * recursion) until the end of the expression string is reached.
 */
static int parse_post_subexpr(struct eval *s, const char *expr, const char *p);
static int parse_subexpr(struct eval *s, const char *expr, const char *p);

static int parse_opening_paren(struct eval *s, const char *expr, const char *p)
{
    p += strspn(p, WHITESPACES);
    if (!*p)
        return 0;

    const size_t pos = (size_t)(p - expr);

    if (*p == '(') {
        const struct token token = {.type=TOKEN_SPECIAL, .pos=pos, .chr='('};
        PUSH(&s->tokens, &token);
        return parse_subexpr(s, expr, p + 1);
    }

    LOG(ERROR, "expected '(' around position %zu", pos);
    return NGL_ERROR_INVALID_DATA;
}

static int parse_subexpr(struct eval *s, const char *expr, const char *p)
{
    p += strspn(p, WHITESPACES);
    if (!*p)
        return 0;

    const size_t pos = (size_t)(p - expr);

    /* Parse unary operators */
    if (*p == '+' || *p == '-') {
        const struct token token = {
            .type       = TOKEN_UNARY_OPERATOR,
            .pos        = pos,
            .chr        = *p,
            .precedence = MAX_PRECEDENCE,
            .func.f     = *p == '-' ? f_negate : f_noop,
            .nb_args    = 1,
        };
        PUSH(&s->tokens, &token);
        return parse_subexpr(s, expr, p + 1);
    }

    /* Parse special '(' */
    if (*p == '(') {
        const struct token token = {.type=TOKEN_SPECIAL, .pos=pos, .chr='('};
        PUSH(&s->tokens, &token);
        return parse_subexpr(s, expr, p + 1);
    }

    /* Parse numbers */
    char *endptr = NULL;
    const float value = strtof(p, &endptr);
    if (p != endptr) {
        const struct token token = {.type=TOKEN_CONSTANT, .pos=pos, .value=value};
        PUSH(&s->tokens, &token);
        return parse_post_subexpr(s, expr, endptr);
    }

    /* At this point the token can only be a string identifier */
    const size_t token_len = strspn(p, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.");
    if (!token_len) {
        LOG(ERROR, "parse error near '%.5s' at position %zu", p, pos);
        return NGL_ERROR_INVALID_DATA;
    }
    char name[MAX_ID_LEN];
    snprintf(name, sizeof(name), "%.*s", (int)token_len, p);
    p += token_len;

    /* Lookup name in variables map */
    if (s->vars) {
        void *data = ngli_hmap_get_str(s->vars, name);
        if (data) {
            const struct token token = {.type=TOKEN_VARIABLE, .pos=pos, .ptr=data};
            PUSH(&s->tokens, &token);
            return parse_post_subexpr(s, expr, p);
        }
    }

    /* Lookup name in constants map */
    void *data = ngli_hmap_get_str(s->consts, name);
    if (data) {
        const struct constant *c = data;
        const struct token token = {.type=TOKEN_CONSTANT, .pos=pos, .value=c->value};
        PUSH(&s->tokens, &token);
        return parse_post_subexpr(s, expr, p);
    }

    /* Lookup name in functions map */
    data = ngli_hmap_get_str(s->funcs, name);
    if (data) {
        const struct function *f = data;
        const struct token token = {
            .type       = TOKEN_FUNCTION,
            .pos        = pos,
            .precedence = MAX_PRECEDENCE,
            .func.f     = f->func,
            .name       = f->name,
            .nb_args    = f->nb_args,
        };
        PUSH(&s->tokens, &token);
        return parse_opening_paren(s, expr, p);
    }

    LOG(ERROR, "unrecognized token '%s' at position %zu", name, pos);
    return NGL_ERROR_INVALID_DATA;
}

static int parse_post_subexpr(struct eval *s, const char *expr, const char *p)
{
    p += strspn(p, WHITESPACES);
    if (!*p)
        return 0;

    const size_t pos = (size_t)(p - expr);

    /* Parse binary operators */
    if (strchr("*/+-", *p)) {
        const struct token token = {
            .type       = TOKEN_BINARY_OPERATOR,
            .pos        = pos,
            .chr        = *p,
            .precedence = get_binary_operator_precedence(*p),
            .func.f     = get_binary_operator_func(*p),
            .nb_args    = 2,
        };
        PUSH(&s->tokens, &token);
        return parse_subexpr(s, expr, p + 1);
    }

    /* Parse special end characters ')' and ',' */
    if (*p == ')') {
        const struct token token = {.type=TOKEN_SPECIAL, .pos=pos, .chr=')'};
        PUSH(&s->tokens, &token);
        return parse_post_subexpr(s, expr, p + 1);
    } else if (*p == ',') {
        const struct token token = {.type=TOKEN_SPECIAL, .pos=pos, .chr=','};
        PUSH(&s->tokens, &token);
        return parse_subexpr(s, expr, p + 1);
    }

    LOG(ERROR, "expected separator around position %zu", pos);
    return NGL_ERROR_INVALID_DATA;
}

/* Tokenization pass: build a list of tokens */
static int tokenize(struct eval *s, const char *expr)
{
    /* Build temporary hash map for fast function lookups */
    s->funcs = ngli_hmap_create(NGLI_HMAP_TYPE_STR);
    if (!s->funcs)
        return NGL_ERROR_MEMORY;
    for (size_t i = 0; i < NGLI_ARRAY_NB(functions_map); i++) {
        int ret = ngli_hmap_set_str(s->funcs, functions_map[i].name, (void *)&functions_map[i]);
        if (ret < 0)
            return ret;
    }

    /* Build temporary hash map for fast constant lookups */
    s->consts = ngli_hmap_create(NGLI_HMAP_TYPE_STR);
    if (!s->consts)
        return NGL_ERROR_MEMORY;
    for (size_t i = 0; i < NGLI_ARRAY_NB(constants_map); i++) {
        int ret = ngli_hmap_set_str(s->consts, constants_map[i].name, (void *)&constants_map[i]);
        if (ret < 0)
            return ret;
    }

    /* Parse the full expression */
    int ret = parse_subexpr(s, expr, expr);
    if (ret < 0)
        return ret;

    /* Pointers and values have been transfered into the tokens so we don't
     * need them anymore */
    ngli_hmap_freep(&s->funcs);
    ngli_hmap_freep(&s->consts);

    return 0;
}

static int missing_argument(const struct token *token, int got)
{
    if (token->type == TOKEN_UNARY_OPERATOR || token->type == TOKEN_BINARY_OPERATOR)
        LOG(ERROR, "missing argument for %s operator '%c' at position %zu, "
            "expected %d but got %d",
            token->type == TOKEN_UNARY_OPERATOR ? "unary" : "binary",
            token->chr, token->pos, token->nb_args, got);
    else if (token->type == TOKEN_FUNCTION)
        LOG(ERROR, "missing argument for function '%s' at position %zu, "
            "expected %d but got %d", token->name, token->pos, token->nb_args, got);
    return NGL_ERROR_INVALID_DATA;
}

/*
 * Check that the operator has the expected number of arguments by consuming
 * the stack.
 */
static int check_operator_pop(struct darray *stack, const struct token *token)
{
    const struct token *o1 = ngli_darray_pop(stack);
    if (!o1)
        return missing_argument(token, 0);
    if (token->nb_args == 1)
        return 0;

    const struct token *o2 = ngli_darray_pop(stack);
    if (!o2)
        return missing_argument(token, 1);
    if (token->nb_args == 2)
        return 0;

    const struct token *o3 = ngli_darray_pop(stack);
    if (!o3)
        return missing_argument(token, 2);
    if (token->nb_args == 3)
        return 0;

    ngli_assert(0);
}

/*
 * This function has multiple purposes:
 *
 * - check if the expression is valid by simulating a simplified evaluation of
 *   the expression with extra checks (which would have been redundant if
 *   called for every eval_run call).
 * - make sure the temporary stack is pre-allocated with enough space so that
 *   eval_run calls do not trigger any heap re-alloc
 */
static int prepare_eval_run(struct eval *s)
{
    struct darray *stack = &s->tmp_stack;

    ngli_darray_clear(stack);

    const struct token *tokens = ngli_darray_data(&s->output);
    for (size_t i = 0; i < ngli_darray_count(&s->output); i++) {
        const struct token *token = &tokens[i];

        if (token->type == TOKEN_CONSTANT || token->type == TOKEN_VARIABLE) {
            PUSH(stack, token);
        } else {
            int ret = check_operator_pop(stack, token);
            if (ret < 0)
                return ret;
            const struct token r = {.type=TOKEN_CONSTANT}; /* fake result */
            PUSH(stack, &r);
        }
    }

    const size_t n = ngli_darray_count(stack);
    if (n > 1) {
        LOG(ERROR, "detected %zu dangling expressions without operators between them", n);
        return NGL_ERROR_INVALID_DATA;
    }

    return 0;
}

/*
 * This function decides if `op` should be processed before `cur`
 */
static int must_be_processed_first(const struct token *op, const struct token *cur)
{
    if (!op || op->chr == '(')
        return 0;
    if (op->type == TOKEN_FUNCTION || op->type == TOKEN_UNARY_OPERATOR)
        return op->precedence > cur->precedence;
    return op->precedence >= cur->precedence;
}

/*
 * RPN pass: translate tokens list expressed in the infix notation into
 * postfix/Reverse Polish Notation using the shunting-yard algorithm
 */
static int infix_to_rpn(struct eval *s, const char *expr)
{
    struct darray *operators = &s->tmp_stack;
    const struct token *tokens = ngli_darray_data(&s->tokens);
    for (size_t i = 0; i < ngli_darray_count(&s->tokens); i++) {
        const struct token *token = &tokens[i];

        if (token->type == TOKEN_CONSTANT || token->type == TOKEN_VARIABLE) {
            PUSH(&s->output, token);
            continue;
        }

        if (token->chr == '(') {
            PUSH(operators, token);
            continue;
        }

        if (token->chr == ')') {
            for (;;) {
                const struct token *o = ngli_darray_pop(operators);
                if (!o) {
                    LOG(ERROR, "expected opening '(' not found for closing ')' at position %zu", token->pos);
                    return NGL_ERROR_INVALID_DATA;
                }
                if (o->chr == '(')
                    break;
                PUSH(&s->output, o);
            }
            continue;
        }

        if (token->chr == ',') {
            for (;;) {
                const struct token *o = ngli_darray_tail(operators);
                if (!o) {
                    LOG(ERROR, "unexpected comma outside a function call (at position %zu)", token->pos);
                    return NGL_ERROR_INVALID_DATA;
                }
                if (o->chr == '(')
                    break;
                PUSH(&s->output, ngli_darray_pop(operators));
            }
            continue;
        }

        /*
         * As long as the operators tail contains token that must be evaluated
         * before the current one, we transfer them to the output stack
         */
        while (must_be_processed_first(ngli_darray_tail(operators), token)) {
            const struct token *o = ngli_darray_pop(operators);
            PUSH(&s->output, o);
        }
        PUSH(operators, token);
    }

    /* Flush remaining operators into the output */
    for (;;) {
        const struct token *token = ngli_darray_pop(operators);
        if (!token)
            break;
        if (token->chr == '(' || token->chr == ')') {
            LOG(ERROR, "unexpected '%c' at position %zu", token->chr, token->pos);
            return NGL_ERROR_INVALID_DATA;
        }
        PUSH(&s->output, token);
    }

    /* Output has been generated so we don't need the input tokens anymore */
    ngli_darray_reset(&s->tokens);

    return prepare_eval_run(s);
}

int ngli_eval_init(struct eval *s, const char *expr, const struct hmap *vars)
{
    if (!expr)
        return NGL_ERROR_INVALID_DATA;

    s->vars = vars;

    int ret;
    if ((ret = tokenize(s, expr)) < 0 ||
        (ret = infix_to_rpn(s, expr)) < 0)
        return ret;

    return 0;
}

/*
 * Evaluate the operator by consuming the stack.
 */
static float eval_operator_pop(struct darray *stack, const struct token *token)
{
    const struct token *o1 = ngli_darray_pop_unsafe(stack);
    if (token->nb_args == 1)
        return token->func.f1(o1->value);

    const struct token *o2 = ngli_darray_pop_unsafe(stack);
    if (token->nb_args == 2)
        return token->func.f2(o2->value, o1->value);

    const struct token *o3 = ngli_darray_pop_unsafe(stack);
    if (token->nb_args == 3)
        return token->func.f3(o3->value, o2->value, o1->value);

    ngli_assert(0);
}

int ngli_eval_run(struct eval *s, float *dst)
{
    struct darray *stack = &s->tmp_stack;

    ngli_darray_clear(stack);

    const struct token *tokens = ngli_darray_data(&s->output);
    for (size_t i = 0; i < ngli_darray_count(&s->output); i++) {
        const struct token *token = &tokens[i];
        if (token->type == TOKEN_VARIABLE) {
            const struct token r = {.type=TOKEN_CONSTANT, .value=*token->ptr};
            PUSH(stack, &r);
        } else if (token->type == TOKEN_CONSTANT) {
            PUSH(stack, token);
        } else {
            const struct token r = {.type=TOKEN_CONSTANT, .value=eval_operator_pop(stack, token)};
            PUSH(stack, &r);
        }
    }

    const struct token *res = ngli_darray_pop(stack);
    *dst = res ? res->value : 0.f;
    return 0;
}

void ngli_eval_freep(struct eval **sp)
{
    struct eval *s = *sp;
    if (!s)
        return;
    ngli_darray_reset(&s->tokens);
    ngli_darray_reset(&s->tmp_stack);
    ngli_darray_reset(&s->output);
    ngli_hmap_freep(&s->funcs);
    ngli_hmap_freep(&s->consts);
    ngli_freep(sp);
}
