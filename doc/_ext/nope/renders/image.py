from pynopegl_utils.misc import load_media

import pynopegl as ngl


@ngl.scene()
def image(cfg: ngl.SceneCfg):
    image = load_media(cfg, "rooster")  # Replace "rooster" with a path to your image

    cfg.aspect_ratio = image.width, image.height

    # Warning: the texture can be shared, but not the media
    media = ngl.Media(image.filename)
    tex = ngl.Texture2D(data_src=media)
    return ngl.RenderTexture(tex)
