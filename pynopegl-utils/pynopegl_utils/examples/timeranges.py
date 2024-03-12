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
    media1 = load_media("mire")
    media2 = load_media("mire")
    m1 = ngl.Media(media1.filename, label="media #1")
    m2 = ngl.Media(media2.filename, label="media #2")

    t1 = ngl.Texture2D(data_src=m1, label="texture #1")
    t2 = ngl.Texture2D(data_src=m2, label="texture #2")

    draw1 = ngl.DrawTexture(t1)
    draw2 = ngl.DrawTexture(t2)

    text_settings = {
        "box": (-1, 1 - 0.2, 2.0, 0.2),
    }
    draw1 = ngl.Group(children=(draw1, ngl.Text("media #1", **text_settings)))
    draw2 = ngl.Group(children=(draw2, ngl.Text("media #2", **text_settings)))

    end_time = 0.0
    rr1 = []
    rr2 = []
    i = 0
    while end_time < cfg.duration:
        (rr, draw) = (rr2, draw2) if i & 1 else (rr1, draw1)
        start_time = i * segment_time
        end_time = (i + 1) * segment_time
        tseg = ngl.TimeRangeFilter(draw, start_time, end_time)
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

    media1 = load_media("mire")
    media2 = load_media("cat")
    m1 = ngl.Media(media1.filename, label="media #1")
    m2 = ngl.Media(media2.filename, label="media #2")

    animkf_m2 = [
        ngl.AnimKeyFrameFloat(transition_start, 0),
        ngl.AnimKeyFrameFloat(transition_start + cfg.duration, cfg.duration),
    ]
    m2.set_time_anim(ngl.AnimatedTime(animkf_m2))

    t1 = ngl.Texture2D(data_src=m1, label="texture #1")
    t2 = ngl.Texture2D(data_src=m2, label="texture #2")

    draw1 = ngl.DrawTexture(t1, label="draw #1")
    draw2 = ngl.DrawTexture(t2, label="draw #2")

    delta_animkf = [
        ngl.AnimKeyFrameFloat(transition_start, 1.0),
        ngl.AnimKeyFrameFloat(transition_start + transition_duration, 0.0),
    ]
    delta = ngl.AnimatedFloat(delta_animkf)

    draw1_2 = ngl.Draw(q, p1_2, label="transition")
    draw1_2.update_frag_resources(tex0=t1, tex1=t2)
    draw1_2.update_frag_resources(delta=delta)

    transition_end = transition_start + transition_duration

    rf1 = ngl.TimeRangeFilter(draw1, start=0, end=transition_start)
    rf2 = ngl.TimeRangeFilter(draw2, start=transition_end, end=None)
    rf1_2 = ngl.TimeRangeFilter(draw1_2, start=transition_start, end=transition_end)

    g = ngl.Group()
    g.add_children(rf1, rf1_2, rf2)

    return g
