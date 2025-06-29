void main()
{
    vec4 iclr = ngl_texvideo(tex0, var_tex0_coord);
    ngl_out_color = texture(lut3d, iclr.rgb);
}
