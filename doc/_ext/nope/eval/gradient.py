import pynopegl as ngl


@ngl.scene()
def gradient(cfg: ngl.SceneCfg, mode="ramp"):
    cfg.duration = 15

    # Create live-controls for the color of each point
    c0_node = ngl.UniformColor(value=(1, 0.5, 0.5), live_id="c0")
    c1_node = ngl.UniformColor(value=(0.5, 1, 0.5), live_id="c1")

    # Make their positions change according to the time
    pos_res = dict(t=ngl.Time())
    pos0 = ngl.EvalVec3("sin( 0.307*t - 0.190)", "sin( 0.703*t - 0.957)", "0", resources=pos_res)
    pos1 = ngl.EvalVec3("sin(-0.236*t + 0.218)", "sin(-0.851*t - 0.904)", "0", resources=pos_res)

    # Represent them with a circle of the "opposite" color (roughly)
    # The scales are here to honor the aspect ratio to prevent circle stretching
    pt0_color = ngl.EvalVec3("1-c.r", "1-c.g", "1-c.b", resources=dict(c=c0_node))
    pt1_color = ngl.EvalVec3("1-c.r", "1-c.g", "1-c.b", resources=dict(c=c1_node))
    geom = ngl.Circle(radius=0.05, npoints=16)
    p0 = ngl.DrawColor(color=pt0_color, geometry=geom)
    p1 = ngl.DrawColor(color=pt1_color, geometry=geom)
    p0 = ngl.Scale(p0, factors=(1 / cfg.aspect_ratio_float, 1, 1))
    p1 = ngl.Scale(p1, factors=(1 / cfg.aspect_ratio_float, 1, 1))
    p0 = ngl.Translate(p0, vector=pos0)
    p1 = ngl.Translate(p1, vector=pos1)

    # Convert the position to 2D points to make them usable in DrawGradient
    pos0_2d = ngl.EvalVec2("p.x/2+.5", ".5-p.y/2", resources=dict(p=pos0))
    pos1_2d = ngl.EvalVec2("p.x/2+.5", ".5-p.y/2", resources=dict(p=pos1))
    grad = ngl.DrawGradient(pos0=pos0_2d, pos1=pos1_2d, mode=mode, color0=c0_node, color1=c1_node)

    return ngl.Group(children=[grad, p0, p1])
