vec3 ngli_srgb2linear(vec3 color)
{
    return mix(
        color / 12.92,
        pow((color + .055) / 1.055, vec3(2.4)),
        step(vec3(0.04045), color)
    );
}
