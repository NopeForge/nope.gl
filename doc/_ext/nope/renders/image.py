from pynopegl_utils.misc import MediaInfo, SceneCfg, scene

import pynopegl as ngl


@scene()
def image(cfg: SceneCfg):
    # Replace the following with MediaInfo.from_filename("/path/to/image.jpg")
    image: MediaInfo = next(m for m in cfg.medias if m.filename.endswith(".jpg"))

    cfg.aspect_ratio = image.width, image.height

    # Warning: the texture can be shared, but not the media
    media = ngl.Media(image.filename)
    tex = ngl.Texture2D(data_src=media)
    return ngl.RenderTexture(tex)
