import array
import math

from pynodegl import (
        AnimKeyFrameFloat,
        AnimatedFloat,
        Media,
        Program,
        Quad,
        Render,
        Texture2D,
        TimeRangeFilter,
        TimeRangeModeCont,
        TimeRangeModeNoop,
        UniformFloat,
)

from pynodegl_utils.misc import scene, get_frag


@scene(uv_corner_x={'type': 'range', 'range': [0,1], 'unit_base': 100},
       uv_corner_y={'type': 'range', 'range': [0,1], 'unit_base': 100},
       uv_width={'type': 'range', 'range': [0,1], 'unit_base': 100},
       uv_height={'type': 'range', 'range': [0,1], 'unit_base': 100},
       progress_bar={'type': 'bool'})
def centered_media(cfg, uv_corner_x=0, uv_corner_y=0, uv_width=1, uv_height=1, progress_bar=True):

    cfg.duration = cfg.medias[0].duration

    q = Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0),
             (uv_corner_x, uv_corner_y), (uv_width, 0), (0, uv_height))
    m = Media(cfg.medias[0].filename)
    t = Texture2D(data_src=m)
    p = Program()
    render = Render(q, p)
    render.update_textures(tex0=t)

    if progress_bar:
        p.set_fragment(get_frag('progress-bar'))
        time_animkf = [AnimKeyFrameFloat(0, 0),
                       AnimKeyFrameFloat(cfg.duration, 1)]
        time = UniformFloat(anim=AnimatedFloat(time_animkf))
        ar = UniformFloat(cfg.aspect_ratio[0] / float(cfg.aspect_ratio[1]))
        render.update_uniforms(time=time, ar=ar)
    return render


@scene(speed={'type': 'range', 'range': [0.01,2], 'unit_base': 1000})
def playback_speed(cfg, speed=1.0):
    media_duration = cfg.medias[0].duration
    initial_seek = 5
    rush_duration = media_duration - initial_seek
    cfg.duration = rush_duration / speed

    q = Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    time_animkf = [AnimKeyFrameFloat(0, initial_seek),
                   AnimKeyFrameFloat(cfg.duration, media_duration)]
    m = Media(cfg.medias[0].filename, time_anim=AnimatedFloat(time_animkf))
    t = Texture2D(data_src=m)
    p = Program()
    render = Render(q, p)
    render.update_textures(tex0=t)
    return render


@scene()
def time_remapping(cfg):
    media_seek = 10
    noop_duration = 1
    prefetch_duration = 2
    freeze_duration = 1
    playback_duration = 5

    range_start = noop_duration + prefetch_duration
    play_start  = range_start   + freeze_duration
    play_stop   = play_start    + playback_duration
    range_stop  = play_stop     + freeze_duration
    duration    = range_stop    + noop_duration

    cfg.duration = duration

    media_animkf = [
        AnimKeyFrameFloat(play_start, media_seek),
        AnimKeyFrameFloat(play_stop,  media_seek + playback_duration),
    ]

    q = Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    m = Media(cfg.medias[0].filename, time_anim=AnimatedFloat(media_animkf))
    t = Texture2D(data_src=m)
    r = Render(q)
    r.update_textures(tex0=t)

    time_ranges = [
        TimeRangeModeNoop(0),
        TimeRangeModeCont(range_start),
        TimeRangeModeNoop(range_stop),
    ]
    rf = TimeRangeFilter(r, ranges=time_ranges, prefetch_time=prefetch_duration)

    return rf
