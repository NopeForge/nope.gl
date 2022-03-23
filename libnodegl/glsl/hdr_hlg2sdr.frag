#include hdr.glsl

void main()
{
    vec4 hdr = ngl_texvideo(tex, var_tex_coord);
    vec3 sdr = bt2020_to_bt709(tonemap(hlg_ootf(hlg_eotf(hdr.rgb))));
    ngl_out_color = vec4(sdr, hdr.a);
}
