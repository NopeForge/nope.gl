from textwrap import dedent

import pynopegl as ngl


@ngl.scene()
def shadertoy(cfg: ngl.SceneCfg):
    cfg.duration = 5

    vert = dedent(
        """\
        void main()
        {
            ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
            uv = ngl_uvcoord;
        }
        """
    )
    frag = dedent(
        """\
        void main()
        {
            vec3 col = 0.5 + 0.5 * cos(t + uv.xyx + vec3(0.0, 2.0, 4.0));
            ngl_out_color = vec4(col, 1.0);
        }
        """
    )

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))  # Full-screen quad
    program = ngl.Program(vertex=vert, fragment=frag)
    program.update_vert_out_vars(uv=ngl.IOVec2())  # Specify the transfer of information between the 2 stages
    draw = ngl.Draw(quad, program)

    draw.update_frag_resources(t=ngl.Time())
    return draw
