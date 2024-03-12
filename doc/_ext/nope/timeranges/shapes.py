import pynopegl as ngl


@ngl.scene()
def shapes(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 6.0

    # Draw the same shape 3 times in a different place
    shape = ngl.Scale(ngl.DrawColor(), factors=(1 / 6, 1 / 6, 1))
    shape_x3 = [ngl.Translate(shape, (x, 0, 0)) for x in (-0.5, 0, 0.5)]

    # Define a different time range for each branch
    range_filters = [
        ngl.TimeRangeFilter(shape_x3[0], start=1, end=4),
        ngl.TimeRangeFilter(shape_x3[1], start=2, end=5),
        ngl.TimeRangeFilter(shape_x3[2], start=3, end=6),
    ]
    return ngl.Group(range_filters)
