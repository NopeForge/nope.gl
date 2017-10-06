#version 100

precision highp float;
varying vec3 var_normal;
void main()
{
    gl_FragColor = vec4(var_normal, 1.0);
}
