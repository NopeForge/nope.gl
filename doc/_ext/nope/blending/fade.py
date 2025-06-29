import pynopegl as ngl
from pynopegl_utils.misc import load_media


@ngl.scene()
def fade(cfg: ngl.SceneCfg):
    image0 = load_media("rooster")
    image1 = load_media("panda")

    cfg.aspect_ratio = image0.width, image0.height
    cfg.duration = 4

    bg_tex = ngl.Texture2D(data_src=ngl.Media(image0.filename))
    bg = ngl.DrawTexture(bg_tex)

    fg_tex = ngl.Texture2D(data_src=ngl.Media(image1.filename))
    fg = ngl.DrawTexture(fg_tex)
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
