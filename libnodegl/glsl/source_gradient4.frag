#define _gradient4(tl, tr, br, bl, uv) mix(mix(tl, tr, uv.x), mix(bl, br, uv.x), uv.y)

vec4 source_gradient4()
{
    vec3 tl = color_tl * opacity_tl;
    vec3 tr = color_tr * opacity_tr;
    vec3 br = color_br * opacity_br;
    vec3 bl = color_bl * opacity_bl;
    float alpha = _gradient4(opacity_tl, opacity_tr, opacity_br, opacity_bl, uv);
    if (linear)
        return vec4(
            ngli_linear2srgb(
                _gradient4(
                    ngli_srgb2linear(tl),
                    ngli_srgb2linear(tr),
                    ngli_srgb2linear(br),
                    ngli_srgb2linear(bl), uv)),
            alpha);
    return vec4(_gradient4(tl, tr, br, bl, uv), alpha);
}
