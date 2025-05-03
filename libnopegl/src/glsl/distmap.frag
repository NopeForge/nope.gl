/*
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

#include helper_misc_utils.glsl

#define close_to_zero(x) (abs(x) < 1e-4) /* XXX too small value leads to float instabilities in many computations */

#define LARGE_FLOAT 1e38

float poly2(float a, float b, float c, float t)          { return  (a * t + b) * t + c; }
float poly3(float a, float b, float c, float d, float t) { return ((a * t + b) * t + c) * t + d; }

vec3 stitch_1_root(int topology, float root, float a, float b, float c)
{
    bool flip = (topology & 1) == 0;
    if (flip)
        topology ^= 0x7; /* 111 */

    /* Normal expected codepath: expected 1 root, got 1 */
    if (topology == 0x1) /* 001 */
        return vec3(root, LARGE_FLOAT, LARGE_FLOAT);

    /*
     * XXX this one is not correct: we are missing a root we can not invent
     * here…
     */
    if (topology == 0x3) /* 011 */
        return vec3(root, root, LARGE_FLOAT);

    /*
     * Got 1 root but expected 3. This is easy to solve: 1 crossing or 3
     * crossing at the same root are equivalent
     */
    return vec3(root);
}

vec3 stitch_2_roots(int topology, vec2 roots, float a, float b, float c)
{
    bool flip = (topology & 1) == 0;
    if (flip)
        topology ^= 0x7; /* 111 */

    /* Normal expected codepath: expected 2 roots, got 2 */
    if (topology == 0x3) /* 011 */
        return vec3(roots.xy, LARGE_FLOAT);

    float da = 3.0 * a, db = 2.0 * b, dc = c;
    float d0 = poly2(da, db, dc, roots.x);
    float d1 = poly2(da, db, dc, roots.y);
    if (topology == 0x1) { /* 001 */
        bool up_down = d0 > d1;
        return vec3(flip != up_down ? roots.xx : roots.yy, LARGE_FLOAT);
    }

    /*
     * We are expecting up+down+up or down+up+down but got only 2 points. In
     * this case we duplicate the point with the horizontal derivate (that
     * is the point with the derivate closest to 0) because with very slight
     * fluctuation change this point could split in 2.
     */
    return abs(d0) < abs(d1) ? roots.xxy : roots.xyy;
}

vec3 stitch_3_roots(int topology, vec3 roots, float a, float b, float c)
{
    bool flip = (topology & 1) == 0;
    if (flip)
        topology ^= 0x7; /* 111 */

    /* Normal expected codepath: expected 3 roots, got 3 */
    if (topology == 0x7) /* 111 */
        return roots;

    /*
     * Got 3 roots but expected only 1. Likely scenario: the roots are outside
     * the [0,1] range, but we can't just exclude outside the boundaries due to
     * float inaccuracies.
     */
    float da = 3.0 * a, db = 2.0 * b, dc = c;
    float d0 = poly2(da, db, dc, roots.x);
    float d1 = poly2(da, db, dc, roots.y);
    bool up_down_up = d0 > d1;
    if (topology == 0x1) {
        /*
         * up+down+up and we need up, or down+up+down and we need down. Out of
         * the 2 extreme roots, we pick the closest to the center.
         */
        if (up_down_up != flip)
            return vec3(abs(0.5 - roots.x) < abs(0.5 - roots.z) ? roots.x : roots.z, LARGE_FLOAT, LARGE_FLOAT);

        /*
         * up+down+up and we need down, or down+up+down and we need up, so it's
         * always the middle root.
         */
        return vec3(roots.y, LARGE_FLOAT, LARGE_FLOAT);
    }

    /*
     * Got 3 roots but expected only 2: we have to pick the appropriate pair
     * according to the topology expectation. If flip is set, we want down+up,
     * otherwise up+down. Depending on whether we are in up+down+up and
     * down+up+down we can deduce whether it's the 1st or the 2nd pair.
     */
    return vec3(flip != up_down_up ? roots.xy : roots.yz, LARGE_FLOAT);
}

/* Complex multiply, divide, inverse */
vec2 c_mul(vec2 a, vec2 b) { return vec2(dot(vec2(a.x, -a.y), b), dot(a.xy, b.yx)); }
vec2 c_div(vec2 a, vec2 b) { return vec2(dot(a, b), dot(vec2(a.y, -a.x), b)) / dot(b, b); }
vec2 c_inv(vec2 z)         { return vec2(z.x, -z.y) / dot(z, z); }

/* Cascading complex polynomial eval of degree 1 to 5 */
vec2 c_poly1(float a, float b,                                     vec2 x) { return a * x + vec2(b, 0.0); }
vec2 c_poly2(float a, float b, float c,                            vec2 x) { return c_mul(c_poly1(a, b,          x), x) + vec2(c, 0.0); }
vec2 c_poly3(float a, float b, float c, float d,                   vec2 x) { return c_mul(c_poly2(a, b, c,       x), x) + vec2(d, 0.0); }
vec2 c_poly4(float a, float b, float c, float d, float e,          vec2 x) { return c_mul(c_poly3(a, b, c, d,    x), x) + vec2(e, 0.0); }
vec2 c_poly5(float a, float b, float c, float d, float e, float f, vec2 x) { return c_mul(c_poly4(a, b, c, d, e, x), x) + vec2(f, 0.0); }

vec2 sum_of_inv(vec2 z0, vec2 z1, vec2 z2, vec2 z3, vec2 z4) { return c_inv(z0 - z1) + c_inv(z0 - z2) + c_inv(z0 - z3) + c_inv(z0 - z4); }
vec2 sum_of_inv(vec2 z0, vec2 z1, vec2 z2, vec2 z3)          { return c_inv(z0 - z1) + c_inv(z0 - z2) + c_inv(z0 - z3); }
vec2 sum_of_inv(vec2 z0, vec2 z1, vec2 z2)                   { return c_inv(z0 - z1) + c_inv(z0 - z2); }

#define XERR 1e-6
#define SMALL_OFF(off) (dot(off, off) <= XERR * XERR)

/* Generated by scripts/aberth-init.py */
#define K3_0 vec2( 0.866025403784439,  0.500000000000000)
#define K3_1 vec2(-0.866025403784438,  0.500000000000000)
#define K3_2 vec2(-0.000000000000000, -1.000000000000000)

int aberth_ehrlich_3(out vec3 roots, float a, float b, float c, float d)
{
    // Initial candidates set mid-way of the tight Cauchy bound estimate
    float r = (1.0 + ngli_max3(
        pow(abs(b/a), 1.0/3.0),
        pow(abs(c/a), 1.0/2.0),
            abs(d/a))) / sqrt(2.0);

    vec2 prv0 = r * K3_0;
    vec2 prv1 = r * K3_1;
    vec2 prv2 = r * K3_2;

    float da = 3.0 * a;
    float db = 2.0 * b;

    for (int m = 0; m < 16; m++) {
        vec2 d0 = c_div(c_poly3(a, b, c, d, prv0), c_poly2(da, db, c, prv0));
        vec2 d1 = c_div(c_poly3(a, b, c, d, prv1), c_poly2(da, db, c, prv1));
        vec2 d2 = c_div(c_poly3(a, b, c, d, prv2), c_poly2(da, db, c, prv2));

        vec2 off0 = c_div(d0, vec2(1.0, 0.0) - c_mul(d0, sum_of_inv(prv0, prv1, prv2)));
        vec2 off1 = c_div(d1, vec2(1.0, 0.0) - c_mul(d1, sum_of_inv(prv1, prv0, prv2)));
        vec2 off2 = c_div(d2, vec2(1.0, 0.0) - c_mul(d2, sum_of_inv(prv2, prv0, prv1)));

        prv0 -= off0;
        prv1 -= off1;
        prv2 -= off2;

        if (SMALL_OFF(off0) && SMALL_OFF(off1) && SMALL_OFF(off2))
            break;
    }

    /* Keep only the real values */
    int nroot = 0;
    if (close_to_zero(prv0.y)) roots[nroot++] = prv0.x;
    if (close_to_zero(prv1.y)) roots[nroot++] = prv1.x;
    if (close_to_zero(prv2.y)) roots[nroot++] = prv2.x;
    return nroot;
}

/* Generated by scripts/aberth-init.py */
#define K4_0 vec2( 0.923879532511287,  0.382683432365090)
#define K4_1 vec2(-0.382683432365090,  0.923879532511287)
#define K4_2 vec2(-0.923879532511287, -0.382683432365090)
#define K4_3 vec2( 0.382683432365090, -0.923879532511287)

int aberth_ehrlich_4(out vec4 roots, float a, float b, float c, float d, float e)
{
    // Initial candidates set mid-way of the tight Cauchy bound estimate
    float r = (1.0 + ngli_max4(
        pow(abs(b/a), 1.0/4.0),
        pow(abs(c/a), 1.0/3.0),
        pow(abs(d/a), 1.0/2.0),
            abs(e/a))) / sqrt(2.0);

    vec2 prv0 = r * K4_0;
    vec2 prv1 = r * K4_1;
    vec2 prv2 = r * K4_2;
    vec2 prv3 = r * K4_3;

    float da = 4.0 * a;
    float db = 3.0 * b;
    float dc = 2.0 * c;

    for (int m = 0; m < 16; m++) {
        vec2 d0 = c_div(c_poly4(a, b, c, d, e, prv0), c_poly3(da, db, dc, d, prv0));
        vec2 d1 = c_div(c_poly4(a, b, c, d, e, prv1), c_poly3(da, db, dc, d, prv1));
        vec2 d2 = c_div(c_poly4(a, b, c, d, e, prv2), c_poly3(da, db, dc, d, prv2));
        vec2 d3 = c_div(c_poly4(a, b, c, d, e, prv3), c_poly3(da, db, dc, d, prv3));

        vec2 off0 = c_div(d0, vec2(1.0, 0.0) - c_mul(d0, sum_of_inv(prv0, prv1, prv2, prv3)));
        vec2 off1 = c_div(d1, vec2(1.0, 0.0) - c_mul(d1, sum_of_inv(prv1, prv0, prv2, prv3)));
        vec2 off2 = c_div(d2, vec2(1.0, 0.0) - c_mul(d2, sum_of_inv(prv2, prv0, prv1, prv3)));
        vec2 off3 = c_div(d3, vec2(1.0, 0.0) - c_mul(d3, sum_of_inv(prv3, prv0, prv1, prv2)));

        prv0 -= off0;
        prv1 -= off1;
        prv2 -= off2;
        prv3 -= off3;

        if (SMALL_OFF(off0) && SMALL_OFF(off1) && SMALL_OFF(off2) && SMALL_OFF(off3))
            break;
    }

    /* Keep only the real values */
    int nroot = 0;
    if (close_to_zero(prv0.y)) roots[nroot++] = prv0.x;
    if (close_to_zero(prv1.y)) roots[nroot++] = prv1.x;
    if (close_to_zero(prv2.y)) roots[nroot++] = prv2.x;
    if (close_to_zero(prv3.y)) roots[nroot++] = prv3.x;
    return nroot;
}

/* Generated by scripts/aberth-init.py */
#define K5_0 vec2( 0.951056516295154,  0.309016994374947)
#define K5_1 vec2( 0.000000000000000,  1.000000000000000)
#define K5_2 vec2(-0.951056516295154,  0.309016994374948)
#define K5_3 vec2(-0.587785252292473, -0.809016994374947)
#define K5_4 vec2( 0.587785252292473, -0.809016994374948)

/*
 * The function returns the real roots of the quintic equation
 * f(x)=ax⁵+bx⁴+cx³+dx²+ex+f. It uses the Aberth–Ehrlich root-finding method,
 * unrolled for the degree 5.
 */
int aberth_ehrlich_5(out float roots[5], float a, float b, float c, float d, float e, float f)
{
    // Initial candidates set mid-way of the tight Cauchy bound estimate
    float r = (1.0 + ngli_max5(
        pow(abs(b/a), 1.0/5.0),
        pow(abs(c/a), 1.0/4.0),
        pow(abs(d/a), 1.0/3.0),
        pow(abs(e/a), 1.0/2.0),
            abs(f/a))) / sqrt(2.0);

    vec2 prv0 = r * K5_0;
    vec2 prv1 = r * K5_1;
    vec2 prv2 = r * K5_2;
    vec2 prv3 = r * K5_3;
    vec2 prv4 = r * K5_4;

    float da = 5.0 * a;
    float db = 4.0 * b;
    float dc = 3.0 * c;
    float dd = 2.0 * d;

    for (int m = 0; m < 16; m++) {
        vec2 d0 = c_div(c_poly5(a, b, c, d, e, f, prv0), c_poly4(da, db, dc, dd, e, prv0));
        vec2 d1 = c_div(c_poly5(a, b, c, d, e, f, prv1), c_poly4(da, db, dc, dd, e, prv1));
        vec2 d2 = c_div(c_poly5(a, b, c, d, e, f, prv2), c_poly4(da, db, dc, dd, e, prv2));
        vec2 d3 = c_div(c_poly5(a, b, c, d, e, f, prv3), c_poly4(da, db, dc, dd, e, prv3));
        vec2 d4 = c_div(c_poly5(a, b, c, d, e, f, prv4), c_poly4(da, db, dc, dd, e, prv4));

        vec2 off0 = c_div(d0, vec2(1.0, 0.0) - c_mul(d0, sum_of_inv(prv0, prv1, prv2, prv3, prv4)));
        vec2 off1 = c_div(d1, vec2(1.0, 0.0) - c_mul(d1, sum_of_inv(prv1, prv0, prv2, prv3, prv4)));
        vec2 off2 = c_div(d2, vec2(1.0, 0.0) - c_mul(d2, sum_of_inv(prv2, prv0, prv1, prv3, prv4)));
        vec2 off3 = c_div(d3, vec2(1.0, 0.0) - c_mul(d3, sum_of_inv(prv3, prv0, prv1, prv2, prv4)));
        vec2 off4 = c_div(d4, vec2(1.0, 0.0) - c_mul(d4, sum_of_inv(prv4, prv0, prv1, prv2, prv3)));

        prv0 -= off0;
        prv1 -= off1;
        prv2 -= off2;
        prv3 -= off3;
        prv4 -= off4;

        if (SMALL_OFF(off0) && SMALL_OFF(off1) && SMALL_OFF(off2) && SMALL_OFF(off3) && SMALL_OFF(off4))
            break;
    }

    /* Keep only the real values */
    int nroot = 0;
    if (close_to_zero(prv0.y)) roots[nroot++] = prv0.x;
    if (close_to_zero(prv1.y)) roots[nroot++] = prv1.x;
    if (close_to_zero(prv2.y)) roots[nroot++] = prv2.x;
    if (close_to_zero(prv3.y)) roots[nroot++] = prv3.x;
    if (close_to_zero(prv4.y)) roots[nroot++] = prv4.x;
    return nroot;
}

/* Linear: f(x)=ax+b */
int root_find1(out float root, float a, float b)
{
    if (close_to_zero(a))
        return 0;
    root = -b / a;
    return 1;
}

/* Quadratic: f(x)=ax²+bx+c */
int root_find2(out vec2 roots, float a, float b, float c)
{
    if (close_to_zero(a))
        return root_find1(roots.x, b, c);

    float m = -b / (2.0 * a);
    float delta = m*m - c / a;
    if (close_to_zero(delta)) {
        roots.x = m;
        return 1;
    }
    if (delta < 0.0)
        return 0;
    float z = sqrt(delta);
    roots = m + vec2(-z, z);
    return 2;
}

/* Cubic: f(x)=ax³+bx²+cx+d */
int root_find3(out vec3 roots, float a, float b, float c, float d)
{
    if (close_to_zero(a))
        return root_find2(roots.xy, b, c, d);
    return aberth_ehrlich_3(roots, a, b, c, d);
}

/* Quartic: f(x)=ax⁴+bx³+cx²+dx+e */
int root_find4(out vec4 roots, float a, float b, float c, float d, float e)
{
    if (close_to_zero(a))
        return root_find3(roots.xyz, b, c, d, e);
    return aberth_ehrlich_4(roots, a, b, c, d, e);
}

/* Quintic: f(x)=ax⁵+bx⁴+cx³+dx²+ex+f */
int root_find5(out float roots[5], float a, float b, float c, float d, float e, float f)
{
    if (close_to_zero(a)) {
        vec4 roots4 = vec4(0.0);
        int nroot = root_find4(roots4, b, c, d, e, f);
        roots = float[5](roots4.x, roots4.y, roots4.z, roots4.w, 0.0);
        return nroot;
    }
    return aberth_ehrlich_5(roots, a, b, c, d, e, f);
}

vec3 root_find3_expected(int topology, float a, float b, float c, float d)
{
    vec3 r = vec3(LARGE_FLOAT);
    int n = root_find3(r, a, b, c, d);
    if (n == 3) {
        if (r.x > r.y) r.xy = r.yx;
        if (r.x > r.z) r.xz = r.zx;
        if (r.y > r.z) r.yz = r.zy;
        return stitch_3_roots(topology, r.xyz, a, b, c);
    }
    if (n == 2) {
        if (r.x > r.y) r.xy = r.yx;
        return stitch_2_roots(topology, r.xy, a, b, c);
    }
    if (n == 1) return stitch_1_root(topology, r.x, a, b, c);
    return vec3(LARGE_FLOAT); // cannot return r because it might have been tainted by root_find3()
}

/* TODO: can we reduce the number of polynomial to evaluate for a given pixel? */
vec4 get_color(vec2 p)
{
    float dist = LARGE_FLOAT;
    int winding_number = 0;
    float area = 0.f;

    int base = 0;
    for (int j = 0; j < beziergroup_count; j++) {

        /*
         * Process a group of polynomials, or sub-shape
         */
        int bezier_count = abs(bezier_counts[j]);
        bool closed = bezier_counts[j] < 0;

        int shape_winding_number = 0;
        float shape_min_dist = LARGE_FLOAT;

        float shape_area = 0.f;

        for (int i = 0; i < bezier_count; i++) {
            vec4 bezier_x = bezier_x_buf[base + i];
            vec4 bezier_y = bezier_y_buf[base + i];
            vec2 p0 = vec2(bezier_x.x, bezier_y.x); // start point
            vec2 p1 = vec2(bezier_x.y, bezier_y.y); // control point 1
            vec2 p2 = vec2(bezier_x.z, bezier_y.z); // control point 2
            vec2 p3 = vec2(bezier_x.w, bezier_y.w); // end point

            shape_area += (p1.x - p0.x) * (p1.y + p0.y);
            shape_area += (p2.x - p1.x) * (p2.y + p1.y);
            shape_area += (p3.x - p2.x) * (p3.y + p2.y);

            /* Bezier cubic points to polynomial coefficients */
            vec2 a = -p0 + 3.0*(p1 - p2) + p3;
            vec2 b = 3.0 * (p0 - 2.0*p1 + p2);
            vec2 c = 3.0 * (p1 - p0);
            vec2 d = p0;

            /* Get smallest distance to current point */
            if (shape_min_dist > 0.0) {
                /*
                 * Calculate coefficients for the derivative D'(t) (degree 5) of D(t)
                 * where D(t) is the distance squared
                 * See https://stackoverflow.com/questions/2742610/closest-point-on-a-cubic-bezier-curve/57315396#57315396
                 */
                float da = 6.0 * dot(a, a);
                float db = 10.0 * dot(a, b);
                float dc = 8.0 * dot(a, c) + 4.0 * dot(b, b);
                float dd = 6.0 * (dot(a, d - p) + dot(b, c));
                float de = 4.0 * (dot(b, d) - dot(b, p)) + 2.0 * dot(c, c);
                float df = 2.0 * (dot(c, d) - dot(c, p));

                float roots_dt[5] = float[5](0.0, 0.0, 0.0, 0.0, 0.0);
                int nb_roots_dt = root_find5(roots_dt, da, db, dc, dd, de, df);

                for (int r = 0; r < nb_roots_dt; r++) {
                    float t = roots_dt[r];
                    if (t < 0.0 || t > 1.0) /* ignore out of bounds roots */
                        continue;

                    vec2 pr = ((a * t + b) * t + c) * t + d;
                    vec2 dp = p - pr;
                    float dist = dot(dp, dp); // length squared
                    shape_min_dist = min(shape_min_dist, dist);
                }

                /* Also include points at t=0 and t=1 */
                vec2 dp0 = p - p0;
                vec2 dp3 = p - p3;
                float mdp = min(dot(dp0, dp0), dot(dp3, dp3));
                shape_min_dist = min(shape_min_dist, mdp);
            }

            /* Winding number */
            if (closed) {
                int signs = int(p0.y < p.y)
                          | int(p1.y < p.y) << 1
                          | int(p2.y < p.y) << 2
                          | int(p3.y < p.y) << 3;
                int b0 = 0x2AAA >> signs & 1;
                int b1 = 0xFB21 >> signs & 1;
                int b2 = 0x5174 >> signs & 1;
                int topology = b0 | b1 << 1 | b2 << 2;

                if (topology == 0x2) /* no crossing */
                    continue;

                vec3 roots = root_find3_expected(topology, a.y, b.y, c.y, d.y - p.y);

                bool flip = (topology & 1) == 0;
                if (flip)
                    topology ^= 0x7;
                int inc = flip ? 1 : -1;

                if ((topology & 0x1) != 0 && poly3(a.x, b.x, c.x, d.x, roots.x) > p.x)
                    shape_winding_number += inc;
                if ((topology & 0x2) != 0 && poly3(a.x, b.x, c.x, d.x, roots.y) > p.x)
                    shape_winding_number -= inc;
                if ((topology & 0x4) != 0 && poly3(a.x, b.x, c.x, d.x, roots.z) > p.x)
                    shape_winding_number += inc;
            }
        }

        float cur_sign = winding_number == 0 ? -1.0 : 1.0;
        float shape_sign = shape_winding_number == 0 ? -1.0 : 1.0;

        winding_number += shape_winding_number;
        float new_sign = winding_number == 0 ? -1.0 : 1.0;

        shape_min_dist = shape_sign * sqrt(shape_min_dist);

        bool orientation_flip = sign(area) != sign(shape_area);

#define IN(x) (x > 0.0)
#define OUT(x) (x < 0.0)

        if (IN(cur_sign) && IN(shape_sign) && IN(new_sign)) {
            dist = max(dist, shape_min_dist); // union
        } else if (area != 0.0 && !orientation_flip && IN(cur_sign) && OUT(shape_sign) && IN(new_sign)) {
            dist = max(dist, shape_min_dist); // union
        } else if (area != 0.0 && !orientation_flip && OUT(cur_sign) && IN(shape_sign) && IN(new_sign)) {
            dist = max(dist, shape_min_dist); // union
        } else {
            dist = min(abs(dist), abs(shape_min_dist));
            dist = new_sign * dist;
        }

        area += shape_area;
        base += bezier_count;
    }

    /* Negative means outside, positive means inside */
    return vec4(vec3(dist), 1.0);
}

void main()
{
    vec2 pos = mix(coords.xy, coords.zw, uv); // Remove the padding
    ngl_out_color = get_color(pos * scale);
}
