#version 100
precision highp float;
uniform mat3 ngl_normal_matrix;
uniform sampler2D tex0_sampler;
varying vec3 var_normal;
varying vec2 var_tex0_coord;
void main() {
    vec4 color = texture2D(tex0_sampler, var_tex0_coord);
    gl_FragColor = vec4(vec3(0.5 + color.rgb * var_normal), 1.0);
}
