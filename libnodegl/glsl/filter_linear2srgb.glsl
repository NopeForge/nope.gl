vec4 filter_linear2srgb(vec4 color, vec2 coords)
{
    return vec4(ngli_linear2srgb(color.rgb), color.a);
}
