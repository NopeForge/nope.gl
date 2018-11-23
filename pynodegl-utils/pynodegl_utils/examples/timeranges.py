import pynodegl as ngl
from pynodegl_utils.misc import scene, get_frag, get_vert


@scene(overlap_time={'type': 'range', 'range': [0, 5], 'unit_base': 10},
       dim={'type': 'range', 'range': [1, 10]})
def queued_medias(cfg, overlap_time=1., dim=3):
    '''Queue of medias, mainly used as a demonstration for the prefetch/release mechanism'''
    qw = qh = 2. / dim
    nb_videos = dim * dim
    tqs = []
    p = ngl.Program()
    for y in range(dim):
        for x in range(dim):
            video_id = y*dim + x
            start = video_id * cfg.duration / nb_videos
            animkf = [ngl.AnimKeyFrameFloat(start, 0)]
            m = ngl.Media(cfg.medias[video_id % len(cfg.medias)].filename, time_anim=ngl.AnimatedFloat(animkf))
            m.set_label('media #%d' % video_id)

            corner = (-1. + x*qw, 1. - (y+1)*qh, 0)
            q = ngl.Quad(corner, (qw, 0, 0), (0, qh, 0))
            t = ngl.Texture2D(data_src=m)

            render = ngl.Render(q, p)
            render.set_label('render #%d' % video_id)
            render.update_textures(tex0=t)

            rf = ngl.TimeRangeFilter(render)
            if start:
                rf.add_ranges(ngl.TimeRangeModeNoop(0))
            rf.add_ranges(ngl.TimeRangeModeCont(start),
                          ngl.TimeRangeModeNoop(start + cfg.duration/nb_videos + overlap_time))

            tqs.append(rf)

    return ngl.Group(children=tqs)


@scene(fast={'type': 'bool'},
       segment_time={'type': 'range', 'range': [0, 10], 'unit_base': 10})
def parallel_playback(cfg, fast=True, segment_time=2.):
    '''
    Parallel media playback, flipping between the two sources.

    The fast version makes sure the textures continue to be updated even though
    they are not displayed. On the other hand, the slow version will update the
    textures only when needed to be displayed, causing potential seek in the
    underlying media, and thus undesired delays.
    '''
    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    p = ngl.Program()

    m1 = ngl.Media(cfg.medias[0].filename, label='media #1')
    m2 = ngl.Media(cfg.medias[0].filename, label='media #2')

    t1 = ngl.Texture2D(data_src=m1, label='texture #1')
    t2 = ngl.Texture2D(data_src=m2, label='texture #2')

    render1 = ngl.Render(q, p, label='render #1')
    render1.update_textures(tex0=t1)
    render2 = ngl.Render(q, p, label='render #2')
    render2.update_textures(tex0=t2)

    rf1 = ngl.TimeRangeFilter(render1)
    rf2 = ngl.TimeRangeFilter(render2)

    t = 0
    rr1 = []
    rr2 = []
    while t < cfg.duration:
        rr1.append(ngl.TimeRangeModeCont(t))
        rr1.append(ngl.TimeRangeModeNoop(t + segment_time))

        rr2.append(ngl.TimeRangeModeNoop(t))
        rr2.append(ngl.TimeRangeModeCont(t + segment_time))

        t += 2 * segment_time

    rf1.add_ranges(*rr1)
    rf2.add_ranges(*rr2)

    g = ngl.Group()
    g.add_children(rf1, rf2)
    if fast:
        g.add_children(t1, t2)
    return g


@scene(transition_start={'type': 'range', 'range': [0, 30]},
       transition_duration={'type': 'range', 'range': [0, 30]})
def simple_transition(cfg, transition_start=2, transition_duration=4):
    '''Fading transition between two medias'''

    cfg.duration = transition_start*2 + transition_duration

    vertex = get_vert('dual-tex')
    fragment = get_frag('tex-mix')

    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    p = ngl.Program()
    p1_2 = ngl.Program(vertex=vertex, fragment=fragment)

    m1 = ngl.Media(cfg.medias[0].filename, label='media #1')
    m2 = ngl.Media(cfg.medias[1 % len(cfg.medias)].filename, label='media #2')

    animkf_m2 = [ngl.AnimKeyFrameFloat(transition_start, 0)]
    m2.set_time_anim(ngl.AnimatedFloat(animkf_m2))

    t1 = ngl.Texture2D(data_src=m1, label='texture #1')
    t2 = ngl.Texture2D(data_src=m2, label='texture #2')

    render1 = ngl.Render(q, p, label='render #1')
    render1.update_textures(tex0=t1)
    render2 = ngl.Render(q, p, label='render #2')
    render2.update_textures(tex0=t2)

    delta_animkf = [ngl.AnimKeyFrameFloat(transition_start, 1.0),
                    ngl.AnimKeyFrameFloat(transition_start + transition_duration, 0.0)]
    delta = ngl.UniformFloat(value=1.0, anim=ngl.AnimatedFloat(delta_animkf))

    render1_2 = ngl.Render(q, p1_2, label='transition')
    render1_2.update_textures(tex0=t1, tex1=t2)
    render1_2.update_uniforms(delta=delta)

    rr1 = []
    rr2 = []
    rr1_2 = []

    rr1.append(ngl.TimeRangeModeNoop(transition_start))

    rr2.append(ngl.TimeRangeModeNoop(0))
    rr2.append(ngl.TimeRangeModeCont(transition_start + transition_duration))

    rr1_2.append(ngl.TimeRangeModeNoop(0))
    rr1_2.append(ngl.TimeRangeModeCont(transition_start))
    rr1_2.append(ngl.TimeRangeModeNoop(transition_start + transition_duration))

    rf1 = ngl.TimeRangeFilter(render1, ranges=rr1)
    rf2 = ngl.TimeRangeFilter(render2, ranges=rr2)
    rf1_2 = ngl.TimeRangeFilter(render1_2, ranges=rr1_2)

    g = ngl.Group()
    g.add_children(rf1, rf1_2, rf2)

    return g
