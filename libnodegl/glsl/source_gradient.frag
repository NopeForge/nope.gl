vec4 source_gradient()
{
    vec3 c0 = color0.rgb * opacity0;
    vec3 c1 = color1.rgb * opacity1;

    float t = 0.0;
    if (mode == 0) { /* ramp */
        vec2 pa = uv - pos0;
        vec2 ba = pos1 - pos0;
        pa.x *= aspect;
        ba.x *= aspect;
        t = dot(pa, ba) / dot(ba, ba);
    } else if (mode == 1) { /* radial */
        vec2 pa = uv - pos0;
        vec2 pb = uv - pos1;
        pa.x *= aspect;
        pb.x *= aspect;
        float len_pa = length(pa);
        t = len_pa / (len_pa + length(pb));
    }

    float a = mix(opacity0, opacity1, t);
    if (linear)
        return vec4(ngli_linear2srgb(mix(ngli_srgb2linear(c0), ngli_srgb2linear(c1), t)), a);
    return vec4(mix(c0, c1, t), a);
}
