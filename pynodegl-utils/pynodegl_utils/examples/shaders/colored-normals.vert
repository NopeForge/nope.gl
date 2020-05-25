#version 100

precision highp float;

attribute vec4 ngl_position;
attribute vec3 ngl_normal;
uniform mat4 ngl_modelview_matrix;
uniform mat4 ngl_projection_matrix;
uniform mat3 ngl_normal_matrix;

varying vec3 var_normal;

void main()
{
    gl_Position = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;
    var_normal = ngl_normal_matrix * ngl_normal;
}
