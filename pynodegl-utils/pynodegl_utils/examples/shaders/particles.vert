void main()
{
    vec4 position = vec4(ngl_position, 1.0) + vec4(positions.data[ngl_instance_index], 0.0);
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * position;
}
