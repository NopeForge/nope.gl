import os
import array

from pynodegl import (
        AnimKeyFrameFloat,
        AnimationFloat,
        BufferVec2,
        BufferVec3,
        Camera,
        Geometry,
        GLState,
        Media,
        Program,
        Quad,
        Render,
        Rotate,
        Texture2D,
        Triangle,
)

from pynodegl_utils.misc import scene

from OpenGL import GL


def load_model(fp):
    index = 0

    vertices = []
    uvs = []
    normals = []

    vertex_indices = []
    uv_indices = []
    normal_indices = []

    indexed_vertices = array.array('f')
    indexed_uvs = array.array('f')
    indexed_normals = array.array('f')

    while True:
        line = fp.readline()
        if line == '':
            break
        line = line.strip()
        fragment = line.split(' ')
        if len(fragment) > 0:
            if fragment[0] == 'v':
                vertices.append((
                    float(fragment[1]),
                    float(fragment[2]),
                    float(fragment[3])
                ))
            elif fragment[0] == 'vt':
                uvs.append((
                    float(fragment[1]),
                    float(fragment[2])
                ))
            elif fragment[0] == 'vn':
                normals.append((
                    float(fragment[1]),
                    float(fragment[2]),
                    float(fragment[3])
                ))
            elif fragment[0] == 'f':
                for i in range(1, 4):
                    indices = fragment[i].split('/')
                    vertex_indices.append(int(indices[0]))
                    uv_indices.append(int(indices[1]))
                    normal_indices.append(int(indices[2]))

    for index in zip(vertex_indices, uv_indices, normal_indices):
        vertex_index, uv_index, normal_index = index

        vertex = vertices[vertex_index - 1]
        uv = uvs[uv_index - 1]
        normal = normals[normal_index - 1]

        indexed_vertices.extend(vertex)
        indexed_uvs.extend(uv)
        indexed_normals.extend(normal)

    return indexed_vertices, indexed_uvs, indexed_normals


fragment = """
#version 100
precision highp float;
uniform mat3 ngl_normal_matrix;
uniform sampler2D tex0_sampler;
varying vec3 var_normal;
varying vec2 var_tex0_coord;
void main() {
    vec4 color = texture2D(tex0_sampler, var_tex0_coord);
    gl_FragColor= vec4(vec3(0.5 + color.rgb * var_normal), 1.0);
}
"""


@scene(model={'type': 'model', 'filter': 'Object files (*.obj)'})
def centered_model_media(cfg, n=0.5, model=None):

    if model is None:
        model = os.path.join(os.path.dirname(__file__), 'data', 'model.obj')

    with open(model) as fp:
        vertices_data, uvs_data, normals_data = load_model(fp)

    vertices = BufferVec3(data=vertices_data)
    texcoords = BufferVec2(data=uvs_data)
    normals = BufferVec3(data=normals_data)

    q = Geometry(vertices, texcoords, normals)
    m = Media(cfg.medias[0].filename)
    t = Texture2D(data_src=m)
    p = Program(fragment=fragment)
    render = Render(q, p)
    render.update_textures(tex0=t)
    render.add_glstates(GLState(GL.GL_DEPTH_TEST, GL.GL_TRUE))

    animkf = [AnimKeyFrameFloat(0, 0),
              AnimKeyFrameFloat(cfg.duration, 360*2)]
    rot = Rotate(render, name="roty", axis=(0, 1, 0), anim=AnimationFloat(animkf))

    camera = Camera(rot)
    camera.set_eye(2.0, 2.0, 2.0)
    camera.set_center(0.0, 0.0, 0.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_perspective(45.0, cfg.aspect_ratio, 1.0, 10.0)

    return camera
