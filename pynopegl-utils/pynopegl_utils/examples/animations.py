import array
import colorsys
import math

from pynopegl_utils.misc import SceneCfg, scene

import pynopegl as ngl


def _block(w, h, color, corner=None):
    block_width = (w, 0, 0)
    block_height = (0, h, 0)
    block_corner = (-w / 2.0, -h / 2.0, 0) if corner is None else corner
    block_quad = ngl.Quad(corner=block_corner, width=block_width, height=block_height)
    block_render = ngl.RenderColor(color=color, geometry=block_quad)
    return block_render


def _easing_split(easing):
    name_split = easing.split(":")
    easing_name = name_split[0]
    args = [float(x) for x in name_split[1:]] if len(name_split) > 1 else None
    return easing_name, args


def _easing_join(easing, args):
    return easing if not args else easing + ":" + ":".join("%g" % x for x in args)


def _get_easing_node(cfg: SceneCfg, easing, curve_zoom, color_program, nb_points=128):
    text_vratio = 1 / 8.0
    graph_hpad_ratio = 1 / 16.0

    area_size = 2.0
    width, height = area_size, area_size
    text_height = text_vratio * height
    pad_height = graph_hpad_ratio * height

    # Colors
    hue = cfg.rng.uniform(0, 0.6)
    color = colorsys.hls_to_rgb(hue, 0.6, 1.0)
    ucolor = ngl.UniformVec3(value=color)
    line_ucolor = ngl.UniformVec3(value=(1, 1, 1))

    # Text legend
    text = ngl.Text(
        text=easing,
        fg_color=color[:3],
        padding=3,
        bg_opacity=1,
        box_corner=(-width / 2.0, height / 2.0 - text_height, 0),
        box_width=(width, 0, 0),
        box_height=(0, text_height, 0),
        label="%s legend" % easing,
    )

    # Graph drawing area (where the curve may overflow)
    graph_size = area_size - text_height - pad_height * 2
    graph_block = _block(
        graph_size, graph_size, (0.15, 0.15, 0.15), corner=(-graph_size / 2, -(graph_size + text_height) / 2, 0)
    )

    # Normed area of the graph
    normed_graph_size = graph_size * curve_zoom
    normed_graph_block = _block(
        normed_graph_size,
        normed_graph_size,
        (0, 0, 0),
        corner=(-normed_graph_size / 2, -(normed_graph_size + text_height) / 2, 0),
    )

    # Curve
    easing_name, easing_args = _easing_split(easing)
    curve_scale_factor = graph_size / area_size * curve_zoom
    vertices_data = array.array("f")
    for i in range(nb_points + 1):
        t = i / float(nb_points)
        v = ngl.easing_evaluate(easing_name, t, easing_args)
        x = curve_scale_factor * (t * width - width / 2.0)
        y = curve_scale_factor * (v * height - height / 2.0)
        y -= text_height / 2.0
        vertices_data.extend([x, y, 0])
    vertices = ngl.BufferVec3(data=vertices_data)
    geometry = ngl.Geometry(vertices, topology="line_strip")
    curve = ngl.Render(geometry, color_program, label="%s curve" % easing)
    curve.update_frag_resources(color=ucolor, opacity=ngl.UniformFloat(1))

    # Value cursor
    y = 2 / 3.0 * pad_height
    x = y * math.sqrt(3)
    cursor_geometry = ngl.Triangle((-x, y, 0), (0, 0, 0), (-x, -y, 0))
    cursor = ngl.RenderColor(color[:3], geometry=cursor_geometry, label="%s cursor" % easing)

    # Horizontal value line
    hline_data = array.array("f", (0, 0, 0, graph_size, 0, 0))
    hline_vertices = ngl.BufferVec3(data=hline_data)
    hline_geometry = ngl.Geometry(hline_vertices, topology="line_strip")
    hline = ngl.Render(hline_geometry, color_program, blending="src_over", label="%s value line" % easing)
    hline.update_frag_resources(color=line_ucolor, opacity=ngl.UniformFloat(0.4))

    # Value animation (cursor + h-line)
    value_x = -graph_size / 2.0
    value_y = (-text_height - normed_graph_size) / 2.0
    value_animkf = (
        ngl.AnimKeyFrameVec3(0, (value_x, value_y, 0)),
        ngl.AnimKeyFrameVec3(cfg.duration, (value_x, value_y + normed_graph_size, 0), easing_name, easing_args),
    )
    value_anim = ngl.Group(children=(hline, cursor))
    value_anim = ngl.Translate(value_anim, vector=ngl.AnimatedVec3(value_animkf), label="%s value anim" % easing)

    # Vertical time line
    vline_data = array.array("f", (0, 0, 0, 0, graph_size, 0))
    vline_vertices = ngl.BufferVec3(data=vline_data)
    vline_geometry = ngl.Geometry(vline_vertices, topology="line_strip")
    vline = ngl.Render(vline_geometry, color_program, blending="src_over", label="%s time line" % easing)
    vline.update_frag_resources(color=line_ucolor, opacity=ngl.UniformFloat(0.4))

    # Time animation (v-line only)
    time_x = -normed_graph_size / 2.0
    time_y = (-text_height - graph_size) / 2.0
    time_animkf = (
        ngl.AnimKeyFrameVec3(0, (time_x, time_y, 0)),
        ngl.AnimKeyFrameVec3(cfg.duration, (time_x + normed_graph_size, time_y, 0)),
    )
    time_anim = ngl.Translate(vline, vector=ngl.AnimatedVec3(time_animkf), label="%s time anim" % easing)

    group = ngl.Group(label="%s block" % easing)
    group.add_children(text, graph_block, normed_graph_block, curve, value_anim, time_anim)
    return group


_easing_specs = (
    # fmt: off
    ("linear",    0, 1.),
    ("quadratic", 3, 1.),
    ("cubic",     3, 1.),
    ("quartic",   3, 1.),
    ("quintic",   3, 1.),
    ("power:7.3", 3, 1.),
    ("sinus",     3, 1.),
    ("exp",       3, 1.),
    ("circular",  3, 1.),
    ("bounce",    1, 1.),
    ("elastic",   1, 0.5),
    ("back",      3, 0.7),
    # fmt: on
)


def _get_easing_list():
    easings = []
    for easing, flags, zoom in _easing_specs:
        versions = []
        if flags & 1:
            versions += ["_in", "_out"]
        if flags & 2:
            versions += ["_in_out", "_out_in"]
        if not flags:
            versions = [""]

        for version in versions:
            base_name, args = _easing_split(easing)
            easing_name = _easing_join(base_name + version, args)
            easings.append((easing_name, zoom))
    return easings


_easing_list = _get_easing_list()
_easing_names = [e[0] for e in _easing_list]


def _get_easing_nodes(cfg: SceneCfg, color_program):
    nb = len(_easing_list)
    nb_rows = int(round(math.sqrt(nb)))
    nb_cols = int(math.ceil(nb / float(nb_rows)))
    cfg.aspect_ratio = (nb_cols, nb_rows)
    scenes = []
    for easing in _easing_list:
        easing_name, zoom = easing
        easing_node = _get_easing_node(cfg, easing_name, zoom, color_program)
        scenes.append(easing_node)
    return ngl.GridLayout(scenes, (nb_cols, nb_rows))


@scene(controls=dict(easing_id=scene.List(choices=["*"] + _easing_names)))
def easings(cfg: SceneCfg, easing_id="*"):
    """Display all the easings (primitive for animation / motion design) at once"""
    cfg.duration = 2.0

    vert_data = cfg.get_vert("color")
    frag_data = cfg.get_frag("color")
    color_program = ngl.Program(vertex=vert_data, fragment=frag_data, label="color")
    color_program.update_vert_out_vars(var_uvcoord=ngl.IOVec2())
    full_block = _block(2, 2, (0.3, 0.3, 0.3))

    group = ngl.Group()
    group.add_children(full_block)
    if easing_id == "*":
        group.add_children(_get_easing_nodes(cfg, color_program))
    else:
        cfg.aspect_ratio = (1, 1)
        easing_index = _easing_names.index(easing_id)
        cfg.rng.seed(easing_index)
        easing, zoom = _easing_list[easing_index]
        easing_node = _get_easing_node(cfg, easing, zoom, color_program)
        group.add_children(easing_node)

    return group
