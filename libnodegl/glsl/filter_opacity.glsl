vec4 filter_opacity(vec4 color, vec2 coords, float opacity)
{
    return vec4(color.rgb, 1.0) * opacity;
}
