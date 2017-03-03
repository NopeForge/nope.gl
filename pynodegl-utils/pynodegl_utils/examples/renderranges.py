from pynodegl import TexturedShape, Quad, Texture, Media, Shader, Group
from pynodegl import UniformSampler, UniformScalar, AttributeVec2, AnimKeyFrameScalar
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
            m = Media(cfg.medias[0].filename, start=start)
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

    m1 = Media(cfg.medias[0].filename, name="media1")
    m2 = Media(cfg.medias[0].filename, name="media2")

    t1 = Texture(data_src=m1, name="texture1")
    t2 = Texture(data_src=m2, name="texture2")

    tshape1 = TexturedShape(q, s, t1, name="texturedshape1")
    tshape2 = TexturedShape(q, s, t2, name="texturedshape2")

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


@scene({'name': 'transition_start', 'type': 'range', 'range': [0, 30]},
       {'name': 'transition_duration', 'type': 'range', 'range': [0, 30]})
def simple_transition(cfg, transition_start=2, transition_duration=4):

    vertex='''
#version 100
attribute vec4 ngl_position;
attribute vec3 ngl_normal;
uniform mat4 ngl_modelview_matrix;
uniform mat4 ngl_projection_matrix;
uniform mat3 ngl_normal_matrix;

attribute vec2 tex0_coords;
uniform mat4 tex0_coords_matrix;
uniform vec2 tex0_dimensions;

attribute vec2 tex1_coords;
uniform mat4 tex1_coords_matrix;
uniform vec2 tex1_dimensions;

varying vec2 var_tex0_coords;
varying vec2 var_tex1_coords;
void main()
{
    gl_Position = ngl_projection_matrix * ngl_modelview_matrix * ngl_position;
    var_tex0_coords = (tex0_coords_matrix * vec4(tex0_coords, 0, 1)).xy;
    var_tex1_coords = (tex1_coords_matrix * vec4(tex1_coords, 0, 1)).xy;
}
'''

    fragment='''
#version 100
precision mediump float;
varying vec2 var_tex0_coords;
varying vec2 var_tex1_coords;
uniform sampler2D tex0_sampler;
uniform sampler2D tex1_sampler;
uniform float delta;
void main(void)
{
    vec4 c1 = texture2D(tex0_sampler, var_tex0_coords);
    vec4 c2 = texture2D(tex1_sampler, var_tex1_coords);
    vec4 c3 = vec4(
        c1.r * delta + c2.r * (1.0 - delta),
        c1.g * delta + c2.g * (1.0 - delta),
        c1.b * delta + c2.b * (1.0 - delta),
        1.0
    );

    gl_FragColor = c3;
}'''

    q = Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    s = Shader()
    s1_2 = Shader(vertex_data=vertex, fragment_data=fragment)

    m1 = Media(cfg.medias[0].filename, name="media1")
    m2 = Media(cfg.medias[0].filename, name="media2", start=transition_start)

    t1 = Texture(data_src=m1, name="texture1")
    t2 = Texture(data_src=m2, name="texture2")

    tshape1 = TexturedShape(q, s, t1, name="texturedshape1")
    tshape2 = TexturedShape(q, s, t2, name="texturedshape2")

    delta = UniformScalar("delta", value=1.0)
    delta.add_animkf(AnimKeyFrameScalar(transition_start, 1.0),
                     AnimKeyFrameScalar(transition_start + transition_duration, 0.0))

    tshape1_2 = TexturedShape(q, s1_2, name="texturedshape1_2")
    tshape1_2.set_texture0(t1)
    tshape1_2.set_texture1(t2)
    tshape1_2.add_uniforms(delta)

    rr1 = []
    rr2 = []
    rr1_2 = []

    rr1.append(RenderRangeNoRender(transition_start))

    rr2.append(RenderRangeNoRender(0))
    rr2.append(RenderRangeContinuous(transition_start + transition_duration))

    rr1_2.append(RenderRangeNoRender(0))
    rr1_2.append(RenderRangeContinuous(transition_start))
    rr1_2.append(RenderRangeNoRender(transition_start + transition_duration))

    tshape1.add_ranges(*rr1)
    tshape2.add_ranges(*rr2)
    tshape1_2.add_ranges(*rr1_2)

    g = Group()
    g.add_children(tshape1, tshape1_2, tshape2)

    return g
