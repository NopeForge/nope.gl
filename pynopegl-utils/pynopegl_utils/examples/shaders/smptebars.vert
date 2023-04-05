void main()
{
    var_uvcoord = ngl_uvcoord;
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
}
