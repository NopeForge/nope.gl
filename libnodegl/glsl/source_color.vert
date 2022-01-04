void main()
{
    ngl_out_pos = projection_matrix * modelview_matrix * vec4(position, 1.0);
    uv = uvcoord;
}
