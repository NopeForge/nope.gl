#version 100
attribute vec4 ngl_position;
attribute vec2 ngl_uvcoord;
attribute vec3 ngl_normal;
uniform mat4 ngl_modelview_matrix;
uniform mat4 ngl_projection_matrix;
uniform mat3 ngl_normal_matrix;

uniform mat4 tex0_coord_matrix;
uniform vec2 tex0_dimensions;

uniform mat4 tex1_coord_matrix;
uniform vec2 tex1_dimensions;

varying vec2 var_tex0_coord;
varying vec2 var_tex1_coord;
void main()
{
    gl_Position = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;
    var_tex0_coord = (tex0_coord_matrix * vec4(ngl_uvcoord, 0, 1)).xy;
    var_tex1_coord = (tex1_coord_matrix * vec4(ngl_uvcoord, 0, 1)).xy;
}
