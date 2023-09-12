from pynopegl_utils.misc import SceneCfg, scene

import pynopegl as ngl


@scene(controls=dict(speed=scene.Range(range=[0.01, 2], unit_base=1000)))
def playback_speed(cfg: SceneCfg, speed=1.0):
    """Adjust media playback speed using animation keyframes"""
    m0 = cfg.medias[0]
    media_duration = m0.duration
    initial_seek = min(media_duration, 5)
    rush_duration = media_duration - initial_seek
    cfg.duration = rush_duration / speed
    cfg.aspect_ratio = (m0.width, m0.height)

    q = ngl.Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    time_animkf = [ngl.AnimKeyFrameFloat(0, initial_seek), ngl.AnimKeyFrameFloat(cfg.duration, media_duration)]
    m = ngl.Media(m0.filename, time_anim=ngl.AnimatedTime(time_animkf))
    t = ngl.Texture2D(data_src=m)
    return ngl.RenderTexture(t, geometry=q)


@scene()
def time_remapping(cfg: SceneCfg):
    """
    Time remapping in the following order:
    - nothing displayed for a while (but media prefetch happening in background)
    - first frame displayed for a while
    - normal playback
    - last frame displayed for a while (even though the media is closed)
    - nothing again until the end
    """
    m0 = cfg.medias[0]

    media_seek = 10
    noop_duration = 2
    prefetch_duration = 2
    freeze_duration = 3
    playback_duration = 5

    range_start = noop_duration + prefetch_duration
    play_start = range_start + freeze_duration
    play_stop = play_start + playback_duration
    range_stop = play_stop + freeze_duration
    duration = range_stop + noop_duration

    cfg.duration = duration
    cfg.aspect_ratio = (m0.width, m0.height)

    media_animkf = [
        ngl.AnimKeyFrameFloat(play_start, media_seek),
        ngl.AnimKeyFrameFloat(play_stop, media_seek + playback_duration),
    ]

    m = ngl.Media(m0.filename, time_anim=ngl.AnimatedTime(media_animkf))
    m.set_nopemd_min_level("verbose")
    t = ngl.Texture2D(data_src=m)
    r = ngl.RenderTexture(t)

    rf = ngl.TimeRangeFilter(r, range_start, range_stop, prefetch_time=prefetch_duration)

    base_string = "media time: {:2g} to {:2g}\nscene time: {:2g} to {:2g}\ntime range: {:2g} to {:2g}".format(
        media_seek, media_seek + playback_duration, play_start, play_stop, range_start, range_stop
    )
    text = ngl.Text(
        base_string, box_height=(0, 0.3, 0), box_corner=(-1, 1 - 0.3, 0), aspect_ratio=cfg.aspect_ratio, halign="left"
    )

    group = ngl.Group()
    group.add_children(rf, text)

    steps = (
        ("default color, nothing yet", 0, noop_duration),
        ("default color, media prefetched", noop_duration, range_start),
        ("first frame", range_start, play_start),
        ("normal playback", play_start, play_stop),
        ("last frame", play_stop, range_stop),
        ("default color, media released", range_stop, duration),
    )

    for i, (description, start_time, end_time) in enumerate(steps):
        text = ngl.Text(
            f"{start_time:g} to {end_time:g}: {description}", aspect_ratio=cfg.aspect_ratio, box_height=(0, 0.2, 0)
        )
        text_rf = ngl.TimeRangeFilter(text, start_time, end_time, label="text-step-%d" % i)
        group.add_children(text_rf)

    return group
