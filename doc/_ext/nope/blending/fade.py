from pynopegl_utils.misc import SceneCfg, scene

import pynopegl as ngl


@scene()
def fade(cfg: SceneCfg):
    jpegs = (m for m in cfg.medias if m.filename.endswith(".jpg"))
    image0 = next(jpegs)
    image1 = next(jpegs)

    cfg.aspect_ratio = image0.width, image0.height
    cfg.duration = 4

    bg_tex = ngl.Texture2D(data_src=ngl.Media(image0.filename))
    bg = ngl.RenderTexture(bg_tex)

    fg_tex = ngl.Texture2D(data_src=ngl.Media(image1.filename))
    fg = ngl.RenderTexture(fg_tex)
    fg.set_blending("src_over")

    animkf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(cfg.duration / 2, 1),
        ngl.AnimKeyFrameFloat(cfg.duration, 0),
    ]

    fg.add_filters(
        ngl.FilterOpacity(ngl.AnimatedFloat(animkf)),
    )

    return ngl.Group(children=[bg, fg])
