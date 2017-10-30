#version 100
precision mediump float;
varying vec2 var_tex0_coord;
varying vec2 var_tex1_coord;
uniform sampler2D tex0_sampler;
uniform sampler2D tex1_sampler;
uniform float delta;
void main(void)
{
    vec4 c1 = texture2D(tex0_sampler, var_tex0_coord);
    vec4 c2 = texture2D(tex1_sampler, var_tex1_coord);
    vec4 c3 = vec4(
        c1.r * delta + c2.r * (1.0 - delta),
        c1.g * delta + c2.g * (1.0 - delta),
        c1.b * delta + c2.b * (1.0 - delta),
        1.0
    );

    gl_FragColor = c3;
}
