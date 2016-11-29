from pynodegl import TexturedShape, Quad, Texture, Media, Shader, Group
from pynodegl import UniformSampler, AttributeVec2
from pynodegl import RenderRangeContinuous, RenderRangeOnce, RenderRangeNoRender

from pynodegl_utils.misc import scene

@scene({'name': 'overlap_time', 'type': 'range', 'range': [0,5], 'unit_base': 10},
       {'name': 'dim', 'type': 'range', 'range': [1,10]})
def queued_medias(cfg, overlap_time=1., dim=3):
    qw = qh = 2. / dim
    nb_videos = dim * dim
    tqs = []
    s = Shader()
    for y in range(dim):
        for x in range(dim):
            video_id = y*dim + x
            start = video_id * cfg.duration / nb_videos
            m = Media(cfg.media_filename, start=start)
            m.set_name('media #%d' % video_id)

            corner = (-1. + x*qw, 1. - (y+1)*qh, 0)
            q = Quad(corner, (qw, 0, 0), (0, qh, 0))
            t = Texture(data_src=m)

            tshape = TexturedShape(q, s, t)
            tshape.set_name('TShape #%d' % video_id)

            if start:
                tshape.add_ranges(RenderRangeNoRender(0))
            tshape.add_ranges(RenderRangeContinuous(start),
                          RenderRangeNoRender(start + cfg.duration/nb_videos + overlap_time))

            tqs.append(tshape)

    return Group(children=tqs)

@scene({'name': 'fast', 'type': 'bool'},
       {'name': 'segment_time', 'type': 'range', 'range': [0,10], 'unit_base': 10})
def parallel_playback(cfg, fast=True, segment_time=2.):
    q = Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    s = Shader()

    m1 = Media(cfg.media_filename)
    m2 = Media(cfg.media_filename)
    m1.set_name("media1")
    m2.set_name("media2")

    t1 = Texture(data_src=m1)
    t2 = Texture(data_src=m2)
    t1.set_name("texture1")
    t2.set_name("texture2")

    tshape1 = TexturedShape(q, s, t1)
    tshape2 = TexturedShape(q, s, t2)
    tshape1.set_name("texturedshape1")
    tshape2.set_name("texturedshape2")

    t = 0
    rr1 = []
    rr2 = []
    while t < cfg.duration:
        rr1.append(RenderRangeContinuous(t))
        rr1.append(RenderRangeNoRender(t + segment_time))

        rr2.append(RenderRangeNoRender(t))
        rr2.append(RenderRangeContinuous(t + segment_time))

        t += 2 * segment_time

    tshape1.add_ranges(*rr1)
    tshape2.add_ranges(*rr2)

    g = Group()
    g.add_children(tshape1, tshape2)
    if fast:
        g.add_children(t1, t2)
    return g
