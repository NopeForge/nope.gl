#version 100
precision highp float;
attribute vec4 ngl_position;
attribute vec2 ngl_uvcoord;
uniform mat4 ngl_modelview_matrix;
uniform mat4 ngl_projection_matrix;
uniform mat4 tex0_coord_matrix;
uniform vec2 tex0_dimensions;
varying vec2 var_uvcoord;
varying vec2 var_tex0_coord;
void main()
{
    gl_Position = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;
    var_uvcoord = ngl_uvcoord;
    var_tex0_coord = (tex0_coord_matrix * vec4(ngl_uvcoord, 0.0, 1.0)).xy;
}
