import pynopegl as ngl
from pynopegl_utils.misc import load_media


@ngl.scene()
def image(cfg: ngl.SceneCfg):
    image = load_media("rooster")  # Replace "rooster" with a path to your image

    cfg.aspect_ratio = image.width, image.height

    # Warning: the texture can be shared, but not the media
    media = ngl.Media(image.filename)
    tex = ngl.Texture2D(data_src=media)
    return ngl.DrawTexture(tex)
