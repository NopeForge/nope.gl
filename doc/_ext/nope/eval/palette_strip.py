import pynopegl as ngl


@ngl.scene()
def palette_strip(cfg: ngl.SceneCfg):
    cols, rows = (7, 5)
    cfg.aspect_ratio = (cols, rows)

    # The 2 colors to interpolate from
    c0_node = ngl.UniformColor(value=(1, 0.5, 0.5), live_id="c0")
    c1_node = ngl.UniformColor(value=(1, 1, 0.5), live_id="c1")

    # The 2 grayscale variants to interpolate from
    l0_node = ngl.EvalVec3("luma(c.r, c.g, c.b)", resources=dict(c=c0_node))
    l1_node = ngl.EvalVec3("luma(c.r, c.g, c.b)", resources=dict(c=c1_node))

    # Create each intermediate color
    c_nodes = []
    l_nodes = []
    for i in range(cols - 4):
        c_node = ngl.EvalVec3(
            "srgbmix(c0.r, c1.r, t)",
            "srgbmix(c0.g, c1.g, t)",
            "srgbmix(c0.b, c1.b, t)",
            resources=dict(c0=c0_node, c1=c1_node, t=ngl.UniformFloat((i + 1) / (cols - 1))),
        )
        l_node = ngl.EvalVec3("luma(c.r, c.g, c.b)", resources=dict(c=c_node))
        c_nodes.append(c_node)
        l_nodes.append(l_node)

    # Grid positioning of DrawColor nodes using GridLayout
    c_nodes = [c0_node] + c_nodes + [c1_node]
    l_nodes = [l0_node] + l_nodes + [l1_node]
    c_nodes = [ngl.DrawColor(node) for node in c_nodes]
    l_nodes = [ngl.DrawColor(node) for node in l_nodes]
    empty = ngl.Identity()
    cells = [empty] * (cols + 1) + c_nodes + [empty] * (cols + 2) + l_nodes
    strips = ngl.GridLayout(cells, size=(cols, rows))

    bg = ngl.DrawColor(color=(0.2, 0.2, 0.2))  # A gray background
    return ngl.Group(children=[bg, strips])
