precision mediump float;

in vec2 var_uvcoord;

uniform sampler2D tex0_sampler;
in vec2 var_tex0_coord;

layout (std430, binding = 0) buffer histogram_buffer {
    uint histr[256];
    uint histg[256];
    uint histb[256];
    uint maximum;
};

out vec4 frag_color;

void main()
{
    vec4 img  = texture(tex0_sampler, var_tex0_coord);
    uint x = clamp(uint(var_uvcoord.x * 255.0 + 0.5), 0U, 255U);
    vec3 rgb = vec3(histr[x], histg[x], histb[x]) / float(maximum);
    float y = var_uvcoord.y;
    vec3 ymax = vec3(1.0/3.0, 2.0/3.0, 1.0);
    vec3 v = rgb * 1.0/3.0; // [0;+oo] -> [0;1/3]
    vec3 yzero = ymax - v;
    vec3 yval = step(y, ymax) * (1.0 - step(y, yzero)); // y <= ymax && y > yzero
    vec4 hcolor = vec4(yval, 1.0);
    frag_color = mix(img, hcolor, .6);
}
