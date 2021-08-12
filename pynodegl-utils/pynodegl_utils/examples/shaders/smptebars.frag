const float inf = 1e30; // useful to make border colors span infinitely on the x-axis

/* top bars */
const vec4 tr0 = vec4(-inf, 1.0, 4.0, 5.0) / 7.0;       // red start columns
const vec4 tg0 = vec4(-inf, 1.0, 2.0, 3.0) / 7.0;       // green start columns
const vec4 tb0 = vec4(-inf, 2.0, 4.0, 6.0) / 7.0;       // blue start columns
const vec4 tr1 = vec4(1.0, 2.0, 5.0, 6.0) / 7.0;        // red end columns
const vec4 tg1 = vec4(1.0, 2.0, 3.0, 4.0) / 7.0;        // green end columns
const vec4 tb1 = vec4(1.0, 3.0, 5.0, inf) / 7.0;        // blue end columns

/* mid bars */
const vec2 mr0 = vec2(2.0, 6.0) / 7.0;                  // red start columns
const vec2 mg0 = vec2(4.0, 6.0) / 7.0;                  // green start columns
const vec4 mb0 = vec4(-inf, 2.0, 4.0, 6.0) / 7.0;       // blue start columns
const vec3 mk0 = vec3(1.0, 3.0, 5.0) / 7.0;             // black start columns
const vec2 mr1 = vec2(3.0, inf) / 7.0;                  // red end columns
const vec2 mg1 = vec2(5.0, inf) / 7.0;                  // green end columns
const vec4 mb1 = vec4(1.0, 3.0, 5.0, inf) / 7.0;        // blue end columns
const vec3 mk1 = vec3(2.0, 4.0, 6.0) / 7.0;             // black end columns

/* pludge (bottom bars) */
const vec2 p_i = vec2(-inf, 1.0) * 5.0 / (4.0 * 7.0);   // -I range
const vec2 p_w = vec2(1.0, 2.0) * 5.0 / (4.0 * 7.0);    // white range
const vec2 p_q = vec2(2.0, 3.0) * 5.0 / (4.0 * 7.0);    // Q range
const vec2 p_k = vec2(3.0 * 5.0 / (4.0 * 7.0), inf);    // black range
const vec2 p_b = vec2(5.0, 6.0) / 7.0;                  // triple black bands range

/* Equivalent to start <= v < end. The end boundary is explicitly excluded to prevent overlapping */
float within(float start, float end, float v) { return clamp(    step(start, v) * (1.0 - step(end, v)),             0.0, 1.0); }
float within(vec2  start, vec2  end, vec2  v) { return clamp(dot(step(start, v) * (1.0 - step(end, v)), vec2(1.0)), 0.0, 1.0); }
float within(vec3  start, vec3  end, vec3  v) { return clamp(dot(step(start, v) * (1.0 - step(end, v)), vec3(1.0)), 0.0, 1.0); }
float within(vec4  start, vec4  end, vec4  v) { return clamp(dot(step(start, v) * (1.0 - step(end, v)), vec4(1.0)), 0.0, 1.0); }

/* SD SMPTE bars following SMPTE EG1-1990 (and SMPTE 259M EG-1 for -I and Q RGB colors) */
vec3 smptebars(vec2 p)
{
    vec4 px4 = vec4(p.x);
    vec3 top = vec3(within(tr0, tr1, px4),                              // top red
                    within(tg0, tg1, px4),                              // top green
                    within(tb0, tb1, px4));                             // top blue
    vec3 mid = mix(vec3(within(mr0, mr1, px4.xy),                       // mid red
                        within(mg0, mg1, px4.xy),                       // mid green
                        within(mb0, mb1, px4)),                         // mid blue
                   vec3(0.1),                                           // mid black
                   step(p.x, within(mk0, mk1, px4.xyz)));               // mix mid bar colors with black
    float black_pos = (p.x - 5.0 / 7.0) * 7.0;                          // triple black band position linear interpolation
    float black_off = (floor(black_pos * 3.0) - 1.0) * 0.04;            // black offsetting: [-4%, 0, +4%]
    vec3 bot = vec3(0.0, 0.2456, 0.4125) * within(p_i.x, p_i.y, p.x)    // pludge -I
             + vec3(1.0)                 * within(p_w.x, p_w.y, p.x)    // pludge white
             + vec3(0.2536, 0.0, 0.4703) * within(p_q.x, p_q.y, p.x)    // pludge Q
             + vec3(0.1 * 0.75)          * within(p_k.x, p_k.y, p.x)    // pludge black
             + vec3(black_off)           * within(p_b.x, p_b.y, p.x);   // pludge black bands
    return mix(0.75 * mix(top, mid, step(2.0 / 3.0, p.y)),              // top + mid color, and apply 75% luma
               bot, step(3.0 / 4.0, p.y));                              // mix with pludge (bottom)
}

/* RGB to YIQ colorspace conversion matrices */
const mat3 rgb2yiq = mat3(0.299, 0.587, 0.114, 0.5959, -0.2746, -0.3213, 0.2115, -0.5227, 0.3112);
const mat3 yiq2rgb = mat3(1.0, 0.956, 0.619, 1.0, -0.272, -0.647, 1.0, -1.106, 1.703);

/* Offset YIQ planes (NTSC signals) instead of RGB for more nostalgic realism */
vec3 yiq_offset(vec3 c0, vec3 c1, vec3 c2)
{
    vec3 c0_yiq = rgb2yiq * c0;
    vec3 c1_yiq = rgb2yiq * c1;
    vec3 c2_yiq = rgb2yiq * c2;
    return yiq2rgb * vec3(c0_yiq.x, c1_yiq.y, c2_yiq.z);
}

void main()
{
    float noisy = step((active_noise + 1.0) / 2.0, active_probability); // random(0, 1) <= proba
    vec3 c0 = smptebars(var_uvcoord + uv_noise_0 * noisy);
    vec3 c1 = smptebars(var_uvcoord + uv_noise_1 * noisy);
    vec3 c2 = smptebars(var_uvcoord + uv_noise_2 * noisy);
    vec3 rgb = yiq_offset(c0, c1, c2);
    ngl_out_color = vec4(rgb, 1.0);
}
