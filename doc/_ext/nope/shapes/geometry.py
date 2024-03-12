import array

import pynopegl as ngl


@ngl.scene()
def geometry(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)

    p0 = (cfg.rng.uniform(-1, 0), cfg.rng.uniform(0, 1), 0)
    p1 = (cfg.rng.uniform(0, 1), cfg.rng.uniform(0, 1), 0)
    p2 = (cfg.rng.uniform(-1, 0), cfg.rng.uniform(-1, 0), 0)
    p3 = (cfg.rng.uniform(0, 1), cfg.rng.uniform(-1, 0), 0)

    vertices = array.array("f", p0 + p1 + p2 + p3)
    uvcoords = array.array("f", p0[:2] + p1[:2] + p2[:2] + p3[:2])

    geometry = ngl.Geometry(
        vertices=ngl.BufferVec3(data=vertices),
        uvcoords=ngl.BufferVec2(data=uvcoords),
        topology="triangle_strip",
    )

    return ngl.DrawColor(geometry=geometry)
