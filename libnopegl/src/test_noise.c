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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "noise.h"
#include "utils.h"

static const struct noise_test {
    struct noise_params p;
    float expected_values[30]; // 3s at 10Hz
} noise_tests[] = {
    {
        /* Simple signal with linear interpolation */
        .p = {
            .amplitude  = 1.f,
            .octaves    = 1,
            .lacunarity = 2.f,
            .gain       = .5f,
            .seed       = 0x1234567,
            .function   = NGLI_NOISE_LINEAR,
        },
        .expected_values = {
            0.000000f,-0.126938f,-0.225668f,-0.296189f,-0.338502f,-0.352606f,-0.338502f,-0.296189f,-0.225668f,-0.126938f,
            0.000000f, 0.020963f, 0.037267f, 0.048913f, 0.055901f, 0.058230f, 0.055901f, 0.048913f, 0.037267f, 0.020963f,
            0.000000f, 0.067128f, 0.119338f, 0.156632f, 0.179008f, 0.186466f, 0.179008f, 0.156632f, 0.119338f, 0.067128f,
        },
    }, {
        /*
         * Check cyclicity: even with multiple octaves, if the lacunarity is
         * round, we will observe the cycle through 0 at every second with the
         * gradient noise (first column).
         */
        .p = {
            .amplitude  = 1.2f,
            .octaves    = 8,
            .lacunarity = 2.f,
            .gain       = .5f,
            .seed       = 0xc474fe39,
            .function   = NGLI_NOISE_CUBIC,
        },
        .expected_values = {
            0.000000f,-0.043850f,-0.030711f,-0.032577f, 0.014327f,-0.008308f, 0.006247f, 0.023553f, 0.081410f,-0.002214f,
            0.000000f,-0.046281f,-0.008353f, 0.006055f,-0.001224f, 0.025979f, 0.104520f, 0.056266f,-0.048626f, 0.021999f,
            0.000000f,-0.042351f,-0.045388f, 0.039898f, 0.029366f, 0.109615f, 0.037113f, 0.015866f,-0.016738f, 0.048401f,
        },
    }, {
        /* Lacunarity and gain slightly offset */
        .p = {
            .amplitude  = 1.f,
            .octaves    = 4,
            .lacunarity = 1.98f,
            .gain       = .56f,
            .seed       = 0,
            .function   = NGLI_NOISE_QUINTIC,
        },
        .expected_values = {
            0.000000f,-0.276658f,-0.356364f,-0.290270f,-0.319912f,-0.226213f,-0.071865f,-0.144200f,-0.138108f,-0.150140f,
           -0.054478f, 0.166980f, 0.080542f, 0.006882f,-0.130795f,-0.248364f,-0.203026f,-0.315853f,-0.274210f,-0.239689f,
            0.018803f, 0.170394f, 0.346100f, 0.345984f, 0.245240f, 0.206418f, 0.207302f, 0.204406f, 0.008591f,-0.074062f,
        },
    },
};

static int run_test(void)
{
    int ret = 0;

    for (size_t k = 0; k < NGLI_ARRAY_NB(noise_tests); k++) {
        const struct noise_test *test = &noise_tests[k];
        const struct noise_params *np = &test->p;

        printf("testing amp:%g oct:%d lac:%g gain:%g seed:0x%08x fn:%d\n",
               np->amplitude, np->octaves, np->lacunarity, np->gain, np->seed, np->function);

        struct noise noise;
        if (ngli_noise_init(&noise, &test->p) < 0)
            return EXIT_FAILURE;

        const size_t nb_values = NGLI_ARRAY_NB(test->expected_values);
        for (size_t i = 0; i < nb_values; i++) {
            const float t = (float)i / 10.f;
            const float gv = ngli_noise_get(&noise, t);
            const float ev = test->expected_values[i];
            if (fabs(gv - ev) > 0.0001) {
                fprintf(stderr, "noise(%f)=%g but expected %g [err:%g]\n", t, gv, ev, fabs(gv - ev));
                ret = EXIT_FAILURE;
            }
        }
    }

    return ret;
}

static const struct noise_params default_params = {
    .amplitude  = 1.0,
    .octaves    = 8,
    .lacunarity = 2.,
    .gain       = .5,
    .seed       = 0x70a21519,
    .function   = NGLI_NOISE_CUBIC,
};

int main(int ac, char **av)
{
    if (ac == 1)
        return run_test();

    const float duration = ac > 1 ? (float)atof(av[1]) : 3.f;
    const float frequency = ac > 2 ? (float)atof(av[2]) : 10.f;
    const struct noise_params np = {
        .amplitude  = ac > 3 ? (float)atof(av[3]) : default_params.amplitude,
        .octaves    = ac > 4 ? atoi(av[4]) : default_params.octaves,
        .lacunarity = ac > 5 ? (float)atof(av[5]) : default_params.lacunarity,
        .gain       = ac > 6 ? (float)atof(av[6]) : default_params.gain,
        .seed       = ac > 7 ? (uint32_t)strtol(av[7], NULL, 0) : default_params.seed,
        .function   = ac > 8 ? atoi(av[8]) : default_params.function,
    };

    /* data can be piped to Gnuplot, for example:
     *   test_noise 15 100 1 1 2 .5 0x50726e67 2 | gnuplot -p -e "p '-' using 1:2 with lines" */
    printf("# duration:%g frq:%g amp:%g oct:%d lac:%g gain:%g seed:0x%08x fn:%d\n",
           duration, frequency, np.amplitude, np.octaves, np.lacunarity, np.gain, np.seed, np.function);

    struct noise noise;
    if (ngli_noise_init(&noise, &np) < 0)
        return EXIT_FAILURE;

    const int nb_values = (int)(duration * frequency);
    for (int i = 0; i < nb_values; i++) {
        const float t = (float)i / frequency;
        printf("%f %f\n", t, ngli_noise_get(&noise, t));
    }

    return 0;
}
