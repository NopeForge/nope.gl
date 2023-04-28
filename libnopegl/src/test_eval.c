/*
 * Copyright 2021-2022 GoPro Inc.
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
#include <stdio.h>
#include <stdlib.h>

#include "eval.h"
#include "hmap.h"
#include "utils.h"

struct test_expr {
    int is_valid;
    const char *str;
    float expected_result;
};

static const struct test_expr expressions[] = {
    /* Invalid expressions, must error out properly */
    {0, "((3+1),/,(4+2))"},             // dangling commas
    {0, "()"},                          // expected something between parenthesis
    {0, "(4.31+)"},                     // missing 2nd argument on binary operator
    {0, "+"},                           // missing argument after unary operator
    {0, "+..4"},                        // unexpected extra digit separator
    {0, ","},                           // dangling comma
    {0, "-"},                           // missing argument after unary operator
    {0, "--+-"},                        // missing argument after unary operators
    {0, "-1.1*(4)+2)"},                 // missing opening parenthesis
    {0, "-3(4+2)"},                     // missing operator before parenthesis
    {0, "/"},                           // missing both arguments on binary operator
    {0, "/4"},                          // missing first argument on binary operator
    {0, "1*"},                          // missing 2nd argument on binary operator
    {0, "1+(/4)"},                      // missing first argument on binary operator
    {0, "3*(-4+(2)"},                   // missing closing parenthesis
    {0, "8**y"},                        // extra operator
    {0, "cos"},                         // missing argument
    {0, "cos()"},                       // missing argument
    {0, "exp2(,,,,)"},                  // missing argument, too many separators
    {0, "hypot(,)"},                    // missing arguments
    {0, "linear(1,x,2,y)"},             // too many arguments
    {0, "log log2(1.0)"},               // missing operator between functions
    {0, "max 4,4"},                     // missing parenthesis
    {0, "max(abs(1-3) round(4.3))"},    // missing comma
    {0, "maxx(1, 2)"},                  // both "max" and "x" are valid, but not together
    {0, "min((3+1) 4*5)"},              // missing comma
    {0, "min(3 4)"},                    // missing comma
    {0, "nope"},                        // unknown symbol
    {0, "sin pi)"},                     // missing opening parenthesis
    {0, "tan(pi"},                      // missing closing parenthesis
    {0, "y z"},                         // missing operator between variables
    {0, "{0}"},                         // invalid symbols

    /* Valid expressions */
    {1, "  - 1", -1.f},
    {1, " +.4", .4f},
    {1, "", 0.f},
    {1, "((3))", 3.f},
    {1, "(-(((-3)+(-4))+(1)))", 6.f},
    {1, "+tan(-sin(+cos(-pi)))", 1.1189396031849523f},
    {1, "--+-1", -1.f},
    {1, "-.777", -.777f},
    {1, "-sin(tau/3)*sign(-e)", 0.8660254037844387f},
    {1, "3 * -(4 + z)", -12.693f},
    {1, "3+-6--+x", -1.766f},
    {1, "5*(3+2)-(1/4+6)*exp(x)", 3.5316133699952523f},
    {1, "9.1 / --(-3+x-2)", -2.41635687732342f},
    {1, "\n", 0.f},
    {1, "close(2/3, 0.666666)", 0.f},
    {1, "close_p(2/3, 0.666666, 1e-5)", 1.f},
    {1, "cos(radians(fract(-4.32)*(45+30.5))) / max(-x--sqrt(3), 4+-+3)", 0.9124060741357061f},
    {1, "degrees(7*tau/5 + pi/2) * exp(-x)", 172.92869110207462f},
    {1, "gte(x,.3) * lt(-11.7,y)", 1.f},
    {1, "hypot(x, y) + pow(z, 3) - abs(y)", 0.10812253647544523f},
    {1, "isnan(0/0)", 1.f},
    {1, "linear(1.3, 5.1, 6.4)", 1.34211f},
    {1, "linear2srgb (srgb2linear( 0.04 )) ", 0.04f},
    {1, "linear2srgb (srgb2linear( 0.7 )) ", 0.7f},
    {1, "linearstep(1.3, 5.1, 6.4)", 1.f},
    {1, "mix(x, 3*(y + 1), z/2 + ceil(cos(3*pi/4)*5)) + .5", 65.002623f},
    {1, "mla(cbrt(phi), sqr(z), cube(e))", 20.1482f},
    {1, "mod_e(7.2, 5.3)", 1.9f},
    {1, "mod_e(-7.2, 5.3)", 3.4f},
    {1, "mod_e(7.2, -5.3)", 1.9f},
    {1, "mod_e(-7.2, -5.3)", 3.4f},
    {1, "mod_f(7.2, 5.3)", 1.9f},
    {1, "mod_f(-7.2, 5.3)", 3.4f},
    {1, "mod_f(7.2, -5.3)", -3.4f},
    {1, "mod_f(-7.2, -5.3)", -1.9f},
    {1, "mod_t(7.2, 5.3)", 1.9f},
    {1, "mod_t(-7.2, 5.3)", -1.9f},
    {1, "mod_t(7.2, -5.3)", 1.9f},
    {1, "mod_t(-7.2, -5.3)", -1.9f},
    {1, "smooth(1.3, 5.1, 6.4)", 0.568814f},
    {1, "smoothstep(1.3, 5.1, 6.4)", 1.f},
    {1, "smoothstep(x, -z, 1/2)", 0.501536f},
    {1, "srgb2linear (linear2srgb( 0.003 )) ", 0.003f},
    {1, "srgb2linear (linear2srgb( 0.8 )) ", 0.8f},
    {1, "z", 0.231f},
};

static const float vars_data[] = {1.234f, -7.9f, 0.231f};

static int test_expr(const struct hmap *vars, const struct test_expr *test_e)
{
    int ret = 0;
    struct eval *e = ngli_eval_create();
    if (!e) {
        ret = -1;
        goto end;
    }

    const char *expr = test_e->str;
    ret = ngli_eval_init(e, expr, vars);
    if (test_e->is_valid) {
        if (ret < 0) {
            fprintf(stderr, "E: \"%s\" is valid but failed to parse\n", expr);
            goto end;
        }
    } else {
        if (ret >= 0) {
            fprintf(stderr, "E: \"%s\" is not valid but didn't fail\n", expr);
            ret = -1;
            goto end;
        }
        printf("\"%s\": failed as expected\n", expr);
        ret = 0;
        goto end;
    }

    float f;
    ret = ngli_eval_run(e, &f);
    if (ret < 0)
        goto end;

    if (fabsf(test_e->expected_result - f) > 0.0001) {
        fprintf(stderr, "E: \"%s = %g\" but got %g\n", expr, test_e->expected_result, f);
        ret = -1;
        goto end;
    }

    printf("[OK] \"%s = %g\"\n", expr, f);

end:
    ngli_eval_freep(&e);
    return ret;
}

int main(int ac, char **av)
{

    struct hmap *vars = ngli_hmap_create();
    if (!vars)
        return 1;

    int ret;
    if ((ret = ngli_hmap_set(vars, "x", (void *)&vars_data[0])) < 0 ||
        (ret = ngli_hmap_set(vars, "y", (void *)&vars_data[1])) < 0 ||
        (ret = ngli_hmap_set(vars, "z", (void *)&vars_data[2])) < 0)
        goto end;

    int failed = 0;
    static const int nb_expr = NGLI_ARRAY_NB(expressions);
    for (int i = 0; i < nb_expr; i++)
        failed += test_expr(vars, &expressions[i]) < 0;

    if (failed) {
        fprintf(stderr, "%d/%d failed test(s)\n", failed, nb_expr);
        ret = 1;
    } else {
        printf("%d/%d tests passing\n", nb_expr, nb_expr);
    }

end:
    ngli_hmap_freep(&vars);
    return ret;
}
