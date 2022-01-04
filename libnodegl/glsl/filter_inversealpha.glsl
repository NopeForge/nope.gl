vec4 filter_inversealpha(vec4 color, vec2 coord)
{
    return vec4(color.rgb, 1. - color.a);
}
