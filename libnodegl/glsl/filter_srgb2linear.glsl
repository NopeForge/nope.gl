vec4 filter_srgb2linear(vec4 color, vec2 coords)
{
    return vec4(ngli_srgb2linear(color.rgb), color.a);
}
