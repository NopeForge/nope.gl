#version 100
precision mediump float;
varying vec2 var_tex0_coord;
uniform sampler2D tex0_sampler;
void main()
{
    gl_FragColor = texture2D(tex0_sampler, var_tex0_coord);
}
