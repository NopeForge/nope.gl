vec3 ngli_linear2srgb(vec3 color)
{
    return mix(
        color * 12.92,
        1.055 * pow(max(color, 0.0), vec3(1. / 2.4)) - .055,
        step(vec3(0.0031308), color)
    );
}
