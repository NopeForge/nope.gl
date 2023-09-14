from pynopegl_utils.misc import SceneCfg, load_media, scene

import pynopegl as ngl


@scene()
def video(cfg: SceneCfg):
    video = load_media(cfg, "cat")  # Replace "cat" with a path to your video

    cfg.aspect_ratio = video.width, video.height
    cfg.duration = max(video.duration, 5)  # Limit to 5 seconds maximum

    # Warning: the Texture can be shared, but not the Media
    media = ngl.Media(video.filename)
    tex = ngl.Texture2D(data_src=media)
    return ngl.RenderTexture(tex)
