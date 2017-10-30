#version 100
precision mediump float;
varying vec2 var_uvcoord;
void main(void)
{
    vec2 c = var_uvcoord;
    gl_FragColor = vec4(c.y-c.x, c.x, 1.0-c.y, 1.0);
}
