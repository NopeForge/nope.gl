from pynopegl_utils.misc import get_shader, load_media

import pynopegl as ngl


@ngl.scene(
    controls=dict(
        fast=ngl.scene.Bool(),
        segment_time=ngl.scene.Range(range=[0.1, 10], unit_base=10),
        constrained_timeranges=ngl.scene.Bool(),
    )
)
def parallel_playback(cfg: ngl.SceneCfg, fast=True, segment_time=2.0, constrained_timeranges=False):
    """
    Parallel media playback, flipping between the two sources.

    The fast version makes sure the textures continue to be updated even though
    they are not displayed. On the other hand, the slow version will update the
    textures only when needed to be displayed, causing potential seek in the
    underlying media, and thus undesired delays.
    """
    media1 = load_media(cfg, "mire")
    media2 = load_media(cfg, "mire")
    m1 = ngl.Media(media1.filename, label="media #1")
    m2 = ngl.Media(media2.filename, label="media #2")

    t1 = ngl.Texture2D(data_src=m1, label="texture #1")
    t2 = ngl.Texture2D(data_src=m2, label="texture #2")

    render1 = ngl.RenderTexture(t1)
    render2 = ngl.RenderTexture(t2)

    text_settings = {
        "box_corner": (-1, 1 - 0.2, 0),
        "box_height": (0, 0.2, 0),
        "aspect_ratio": cfg.aspect_ratio,
    }
    render1 = ngl.Group(children=(render1, ngl.Text("media #1", **text_settings)))
    render2 = ngl.Group(children=(render2, ngl.Text("media #2", **text_settings)))

    end_time = 0.0
    rr1 = []
    rr2 = []
    i = 0
    while end_time < cfg.duration:
        (rr, render) = (rr2, render2) if i & 1 else (rr1, render1)
        start_time = i * segment_time
        end_time = (i + 1) * segment_time
        tseg = ngl.TimeRangeFilter(render, start_time, end_time)
        if constrained_timeranges:
            tseg.set_prefetch_time(segment_time / 3)
        rr.append(tseg)
        i += 1

    g = ngl.Group()
    g.add_children(*rr1, *rr2)
    if fast:
        g.add_children(t1, t2)
    return g


@ngl.scene(
    controls=dict(
        transition_start=ngl.scene.Range(range=[0, 30]),
        transition_duration=ngl.scene.Range(range=[0, 30]),
    ),
)
def simple_transition(cfg: ngl.SceneCfg, transition_start=2, transition_duration=4):
    """Fading transition between two medias"""

    cfg.duration = transition_start * 2 + transition_duration

    vertex = get_shader("dual-tex.vert")
    fragment = get_shader("tex-mix.frag")

    q = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    p1_2 = ngl.Program(vertex=vertex, fragment=fragment)
    p1_2.update_vert_out_vars(var_tex0_coord=ngl.IOVec2(), var_tex1_coord=ngl.IOVec2())

    media1 = load_media(cfg, "mire")
    media2 = load_media(cfg, "cat")
    m1 = ngl.Media(media1.filename, label="media #1")
    m2 = ngl.Media(media2.filename, label="media #2")

    animkf_m2 = [
        ngl.AnimKeyFrameFloat(transition_start, 0),
        ngl.AnimKeyFrameFloat(transition_start + cfg.duration, cfg.duration),
    ]
    m2.set_time_anim(ngl.AnimatedTime(animkf_m2))

    t1 = ngl.Texture2D(data_src=m1, label="texture #1")
    t2 = ngl.Texture2D(data_src=m2, label="texture #2")

    render1 = ngl.RenderTexture(t1, label="render #1")
    render2 = ngl.RenderTexture(t2, label="render #2")

    delta_animkf = [
        ngl.AnimKeyFrameFloat(transition_start, 1.0),
        ngl.AnimKeyFrameFloat(transition_start + transition_duration, 0.0),
    ]
    delta = ngl.AnimatedFloat(delta_animkf)

    render1_2 = ngl.Render(q, p1_2, label="transition")
    render1_2.update_frag_resources(tex0=t1, tex1=t2)
    render1_2.update_frag_resources(delta=delta)

    rr1 = []
    rr2 = []
    rr1_2 = []

    transition_end = transition_start + transition_duration

    rf1 = ngl.TimeRangeFilter(render1, start=0, end=transition_start)
    rf2 = ngl.TimeRangeFilter(render2, start=transition_end, end=None)
    rf1_2 = ngl.TimeRangeFilter(render1_2, start=transition_start, end=transition_end)

    g = ngl.Group()
    g.add_children(rf1, rf1_2, rf2)

    return g
