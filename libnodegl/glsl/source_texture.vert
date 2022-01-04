void main()
{
    ngl_out_pos = projection_matrix * modelview_matrix * vec4(position, 1.0);
    uv = uvcoord;
    tex_coord = (tex_coord_matrix * vec4(uvcoord, 0.0, 1.0)).xy;
}
