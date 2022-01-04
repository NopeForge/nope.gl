vec4 filter_premult(vec4 color, vec2 coords)
{
    return color * color.a;
}
