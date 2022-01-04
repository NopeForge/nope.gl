vec4 filter_saturation(vec4 color, vec2 coords, float saturation)
{
    color.rgb = ngli_sat(mix(vec3(dot(ngli_luma_weights, color.rgb)), color.rgb, saturation));
    return color;
}
