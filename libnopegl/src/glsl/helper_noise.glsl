/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2023 Clément Bœsch <u pkh.me>
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

/*
 * Return a random single-precision float between [0,1)
 * Derived from http://prng.di.unimi.it/
 */
highp float u32tof32(highp uint x)
{
    uint y = (0x7FU << 23U) | (x >> 9U);
    return uintBitsToFloat(y) - 1.0;
}

/*
 * lowbias32 hashing from https://nullprogram.com/blog/2018/07/31/
 */
highp uint hash(highp uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

highp uint hash(highp uvec2 x)
{
    return hash(x.x ^ hash(x.y));
}

highp uint hash(highp uvec3 x)
{
    return hash(x.x ^ hash(x.y ^ hash(x.z)));
}

highp float random(highp ivec3 x)
{
    return u32tof32(hash(uvec3(x)));
}

highp vec2 random2(highp ivec3 x)
{
    /* Generate 2 random floats in [0,1] using 16-bit of information for each */
    uint h = hash(uvec3(x));
    float r0 = u32tof32(0x00010001U * (h      & 0xffffU));
    float r1 = u32tof32(0x00010001U * (h>>16  & 0xffffU));
    return vec2(r0, r1);
}

/*
 * Generate a random point on a sphere (with uniform distribution)
 * 
 * References:
 * - https://web.archive.org/web/20200618040237/https://mrl.nyu.edu/~perlin/paper445.pdf
 * - https://web.archive.org/web/20180616011332/http://www.scratchapixel.com/lessons/procedural-generation-virtual-worlds%20/perlin-noise-part-2
 */
highp vec3 random_grad(highp ivec3 x)
{
    vec2 r = random2(x);
    // use the first random for the polar angle (latitude)
    float c = 2.0*r.x - 1.0,    // c = cos(theta) = cos(acos(2x-1)) = 2x-1
          s = sqrt(1.0 - c*c);  // s = sin(theta) = sin(acos(c)) = sqrt(1-c*c)
    float phi = ngli_tau * r.y; // use the 2nd random for the azimuth (longitude)
    return vec3(cos(phi) * s, sin(phi) * s, c);
}

highp vec3 curve_quintic(highp vec3 t)
{
    return ((6. * t - 15.) * t + 10.) * t * t * t;
}

float noise_perlin(vec3 t, uint seed)
{
    vec3 i = floor(t);
    vec3 f = fract(t);
    ivec3 x = ivec3(i) + int(seed);

    float y0 = dot(random_grad(x + ivec3(0, 0, 0)), f - vec3(0, 0, 0));
    float y1 = dot(random_grad(x + ivec3(1, 0, 0)), f - vec3(1, 0, 0));
    float y2 = dot(random_grad(x + ivec3(0, 1, 0)), f - vec3(0, 1, 0));
    float y3 = dot(random_grad(x + ivec3(1, 1, 0)), f - vec3(1, 1, 0));
    float y4 = dot(random_grad(x + ivec3(0, 0, 1)), f - vec3(0, 0, 1));
    float y5 = dot(random_grad(x + ivec3(1, 0, 1)), f - vec3(1, 0, 1));
    float y6 = dot(random_grad(x + ivec3(0, 1, 1)), f - vec3(0, 1, 1));
    float y7 = dot(random_grad(x + ivec3(1, 1, 1)), f - vec3(1, 1, 1));

    vec3 a = curve_quintic(f);
    return mix(mix(mix(y0, y1, a.x), mix(y2, y3, a.x), a.y),
               mix(mix(y4, y5, a.x), mix(y6, y7, a.x), a.y),
               a.z);
}

/* Adapted from https://en.wikipedia.org/wiki/Bicubic_interpolation */
float bicubic(float a, float b, float c, float d, float t)
{
    mat4 m = 0.5 * mat4(
        0.,  2.,  0.,  0.,
       -1.,  0.,  1.,  0.,
        2., -5.,  4., -1.,
       -1.,  3., -3.,  1.
    );
    return dot(m * vec4(1., t, t*t, t*t*t), vec4(a, b, c, d));
}

float noise_blocky(vec3 t, uint seed)
{
    ivec2 xy = ivec2(t.xy) + int(seed);

    /*
     * For each lattice coordinates (x, y) we determine a random frequency and
     * offset that will be used to compute the evolution. For that, we use the
     * lattice coordinates (x, y) that we offset and a fixed seed as third
     * dimension so the result is not correlated directly to (x, y) and to the
     * temporal dimension (t.z). Offseting (x, y) also has the advantage to
     * avoid the case where the evolution is derived from random2(uvec3(0, 0,
     * 0)) which equals to 0.
     */
    vec2 r = random2(ivec3(xy + 1 << 16, int(seed)));
    float evolution_freq = r.x;
    float evolution_offset = r.y;

    /* Apply frequency and offset to the third (temporal) dimension */
    float evolution = t.z * evolution_freq + evolution_offset;
    int evolution_i = int(evolution);
    float evolution_f = fract(evolution);

    float y0 = random(ivec3(xy, evolution_i - 1));
    float y1 = random(ivec3(xy, evolution_i + 0));
    float y2 = random(ivec3(xy, evolution_i + 1));
    float y3 = random(ivec3(xy, evolution_i + 2));

    float res = bicubic(y0, y1, y2, y3, evolution_f);

    /* Scale noise from the [0,1] range to [-1,1] */
    return res * 2.0 - 1.0;
}

float fbm(vec3 xyz, int type, float amplitude, uint octaves, float lacunarity, float gain, uint seed)
{
    float sum = 0.0;
    float amp = amplitude;

    for(uint i = 0U; i < octaves; i++) {
        if (type == 0)
            sum += amp * noise_blocky(xyz, seed);
        else
            sum += amp * noise_perlin(xyz, seed);
        xyz *= lacunarity;
        amp *= gain;
    }

    return sum;
}
