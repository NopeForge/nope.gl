#version 100
precision highp float;
attribute vec4 ngl_position;
uniform mat4 ngl_modelview_matrix;
uniform mat4 ngl_projection_matrix;
void main()
{
    gl_Position = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;
}
