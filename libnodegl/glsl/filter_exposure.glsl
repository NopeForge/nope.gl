vec4 filter_exposure(vec4 color, vec2 coords, float exposure)
{
    color.rgb = ngli_sat(color.rgb * exp2(exposure));
    return color;
}
