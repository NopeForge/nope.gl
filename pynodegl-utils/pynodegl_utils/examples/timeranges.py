from pynodegl import (
        AnimKeyFrameFloat,
        AnimatedFloat,
        Group,
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

from pynodegl_utils.misc import scene, get_frag, get_vert

@scene(overlap_time={'type': 'range', 'range': [0,5], 'unit_base': 10},
       dim={'type': 'range', 'range': [1,10]})
def queued_medias(cfg, overlap_time=1., dim=3):
    qw = qh = 2. / dim
    nb_videos = dim * dim
    tqs = []
    p = Program()
    for y in range(dim):
        for x in range(dim):
            video_id = y*dim + x
            start = video_id * cfg.duration / nb_videos
            animkf = [AnimKeyFrameFloat(start, 0)]
            m = Media(cfg.medias[video_id % len(cfg.medias)].filename, time_anim=AnimatedFloat(animkf))
            m.set_name('media #%d' % video_id)

            corner = (-1. + x*qw, 1. - (y+1)*qh, 0)
            q = Quad(corner, (qw, 0, 0), (0, qh, 0))
            t = Texture2D(data_src=m)

            render = Render(q, p)
            render.set_name('render #%d' % video_id)
            render.update_textures(tex0=t)

            rf = TimeRangeFilter(render)
            if start:
                rf.add_ranges(TimeRangeModeNoop(0))
            rf.add_ranges(TimeRangeModeCont(start),
                          TimeRangeModeNoop(start + cfg.duration/nb_videos + overlap_time))

            tqs.append(rf)

    return Group(children=tqs)

@scene(fast={'type': 'bool'},
       segment_time={'type': 'range', 'range': [0,10], 'unit_base': 10})
def parallel_playback(cfg, fast=True, segment_time=2.):
    q = Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    p = Program()

    m1 = Media(cfg.medias[0].filename, name="media #1")
    m2 = Media(cfg.medias[0].filename, name="media #2")

    t1 = Texture2D(data_src=m1, name="texture #1")
    t2 = Texture2D(data_src=m2, name="texture #2")

    render1 = Render(q, p, name="render #1")
    render1.update_textures(tex0=t1)
    render2 = Render(q, p, name="render #2")
    render2.update_textures(tex0=t2)

    rf1 = TimeRangeFilter(render1)
    rf2 = TimeRangeFilter(render2)

    t = 0
    rr1 = []
    rr2 = []
    while t < cfg.duration:
        rr1.append(TimeRangeModeCont(t))
        rr1.append(TimeRangeModeNoop(t + segment_time))

        rr2.append(TimeRangeModeNoop(t))
        rr2.append(TimeRangeModeCont(t + segment_time))

        t += 2 * segment_time

    rf1.add_ranges(*rr1)
    rf2.add_ranges(*rr2)

    g = Group()
    g.add_children(rf1, rf2)
    if fast:
        g.add_children(t1, t2)
    return g


@scene(transition_start={'type': 'range', 'range': [0, 30]},
       transition_duration={'type': 'range', 'range': [0, 30]})
def simple_transition(cfg, transition_start=1, transition_duration=4):

    cfg.duration = transition_start*2 + transition_duration

    vertex = get_vert('dual-tex')
    fragment = get_frag('tex-mix')

    q = Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    p = Program()
    p1_2 = Program(vertex=vertex, fragment=fragment)

    m1 = Media(cfg.medias[0].filename, name="media #1")
    m2 = Media(cfg.medias[1 % len(cfg.medias)].filename, name="media #2")

    animkf_m2 = [AnimKeyFrameFloat(transition_start, 0)]
    m2.set_time_anim(AnimatedFloat(animkf_m2))

    t1 = Texture2D(data_src=m1, name="texture #1")
    t2 = Texture2D(data_src=m2, name="texture #2")

    render1 = Render(q, p, name="render #1")
    render1.update_textures(tex0=t1)
    render2 = Render(q, p, name="render #2")
    render2.update_textures(tex0=t2)

    delta_animkf = [AnimKeyFrameFloat(transition_start, 1.0),
                    AnimKeyFrameFloat(transition_start + transition_duration, 0.0)]
    delta = UniformFloat(value=1.0, anim=AnimatedFloat(delta_animkf))

    render1_2 = Render(q, p1_2, name="transition")
    render1_2.update_textures(tex0=t1, tex1=t2)
    render1_2.update_uniforms(delta=delta)

    rr1 = []
    rr2 = []
    rr1_2 = []

    rr1.append(TimeRangeModeNoop(transition_start))

    rr2.append(TimeRangeModeNoop(0))
    rr2.append(TimeRangeModeCont(transition_start + transition_duration))

    rr1_2.append(TimeRangeModeNoop(0))
    rr1_2.append(TimeRangeModeCont(transition_start))
    rr1_2.append(TimeRangeModeNoop(transition_start + transition_duration))

    rf1 = TimeRangeFilter(render1, ranges=rr1)
    rf2 = TimeRangeFilter(render2, ranges=rr2)
    rf1_2 = TimeRangeFilter(render1_2, ranges=rr1_2)

    g = Group()
    g.add_children(rf1, rf1_2, rf2)

    return g
