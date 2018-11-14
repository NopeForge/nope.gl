import pynodegl as ngl
from pynodegl_utils.misc import scene, get_frag


@scene(uv_corner_x={'type': 'range', 'range': [0, 1], 'unit_base': 100},
       uv_corner_y={'type': 'range', 'range': [0, 1], 'unit_base': 100},
       uv_width={'type': 'range', 'range': [0, 1], 'unit_base': 100},
       uv_height={'type': 'range', 'range': [0, 1], 'unit_base': 100},
       progress_bar={'type': 'bool'})
def centered_media(cfg, uv_corner_x=0, uv_corner_y=0, uv_width=1, uv_height=1, progress_bar=True):
    '''A simple centered media with an optional progress bar in the shader'''
    m0 = cfg.medias[0]
    cfg.duration = m0.duration
    cfg.aspect_ratio = (m0.width, m0.height)

    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0),
                 (uv_corner_x, uv_corner_y), (uv_width, 0), (0, uv_height))
    m = ngl.Media(m0.filename)
    t = ngl.Texture2D(data_src=m)
    p = ngl.Program()
    render = ngl.Render(q, p)
    render.update_textures(tex0=t)

    if progress_bar:
        p.set_fragment(get_frag('progress-bar'))
        time_animkf = [ngl.AnimKeyFrameFloat(0, 0),
                       ngl.AnimKeyFrameFloat(cfg.duration, 1)]
        time = ngl.UniformFloat(anim=ngl.AnimatedFloat(time_animkf))
        ar = ngl.UniformFloat(cfg.aspect_ratio_float)
        render.update_uniforms(time=time, ar=ar)
    return render


@scene(speed={'type': 'range', 'range': [0.01, 2], 'unit_base': 1000})
def playback_speed(cfg, speed=1.0):
    '''Adjust media playback speed using animation keyframes'''
    m0 = cfg.medias[0]
    media_duration = m0.duration
    initial_seek = 5
    rush_duration = media_duration - initial_seek
    cfg.duration = rush_duration / speed
    cfg.aspect_ratio = (m0.width, m0.height)

    q = ngl.Quad((-0.5, -0.5, 0), (1, 0, 0), (0, 1, 0))
    time_animkf = [ngl.AnimKeyFrameFloat(0, initial_seek),
                   ngl.AnimKeyFrameFloat(cfg.duration, media_duration)]
    m = ngl.Media(m0.filename, time_anim=ngl.AnimatedFloat(time_animkf))
    t = ngl.Texture2D(data_src=m)
    p = ngl.Program()
    render = ngl.Render(q, p)
    render.update_textures(tex0=t)
    return render


@scene()
def time_remapping(cfg):
    '''
    Time remapping in the following order:
    - nothing displayed for a while (but media prefetch happening in background)
    - first frame displayed for a while
    - normal playback
    - last frame displayed for a while (even though the media is closed)
    - nothing again until the end
    '''
    m0 = cfg.medias[0]

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
    cfg.aspect_ratio = (m0.width, m0.height)

    media_animkf = [
        ngl.AnimKeyFrameFloat(play_start, media_seek),
        ngl.AnimKeyFrameFloat(play_stop,  media_seek + playback_duration),
    ]

    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    m = ngl.Media(m0.filename, time_anim=ngl.AnimatedFloat(media_animkf))
    t = ngl.Texture2D(data_src=m)
    r = ngl.Render(q)
    r.update_textures(tex0=t)

    time_ranges = [
        ngl.TimeRangeModeNoop(0),
        ngl.TimeRangeModeCont(range_start),
        ngl.TimeRangeModeNoop(range_stop),
    ]
    rf = ngl.TimeRangeFilter(r, ranges=time_ranges, prefetch_time=prefetch_duration)

    return rf
