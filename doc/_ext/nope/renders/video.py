from pynopegl_utils.misc import MediaInfo, SceneCfg, scene

import pynopegl as ngl


@scene()
def video(cfg: SceneCfg):
    # Replace the following with MediaInfo.from_filename("/path/to/video.mp4")
    video: MediaInfo = next(m for m in cfg.medias if m.filename.endswith(".mp4"))

    cfg.aspect_ratio = video.width, video.height
    cfg.duration = max(video.duration, 5)  # Limit to 5 seconds maximum

    # Warning: the Texture can be shared, but not the Media
    media = ngl.Media(video.filename)
    tex = ngl.Texture2D(data_src=media)
    return ngl.RenderTexture(tex)
