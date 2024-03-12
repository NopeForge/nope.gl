import pynopegl as ngl


@ngl.scene()
def chained_transforms(cfg: ngl.SceneCfg):
    cfg.duration = 5
    cfg.aspect_ratio = (1, 1)

    group = ngl.Group()
    radius = 0.2
    n = 10
    step = 360 / n

    shape = ngl.Circle(radius=radius, npoints=128)
    draw = ngl.DrawColor(geometry=shape)

    for i in range(n):
        mid_time = cfg.duration / 2.0
        start_time = mid_time / (i + 2)
        end_time = cfg.duration - start_time

        scale_animkf = [
            ngl.AnimKeyFrameVec3(start_time, (0, 0, 0)),
            ngl.AnimKeyFrameVec3(mid_time, (1, 1, 1), "exp_out"),
            ngl.AnimKeyFrameVec3(end_time, (0, 0, 0), "exp_in"),
        ]

        angle = i * step
        rotate_animkf = [
            ngl.AnimKeyFrameFloat(start_time, 0),
            ngl.AnimKeyFrameFloat(mid_time, angle, "exp_out"),
            ngl.AnimKeyFrameFloat(end_time, 0, "exp_in"),
        ]

        tnode = draw
        tnode = ngl.Scale(tnode, factors=ngl.AnimatedVec3(scale_animkf))
        tnode = ngl.Translate(tnode, vector=(1 - radius, 0, 0))
        tnode = ngl.Rotate(tnode, angle=ngl.AnimatedFloat(rotate_animkf))

        group.add_children(tnode)

    return group
