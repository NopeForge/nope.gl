from pynodegl import TexturedShape, Quad, Triangle, Shape, ShapePrimitive, Texture, Media, Shader, GLState, Camera, Rotate, AnimKeyFrameScalar

from pynodegl_utils.misc import scene

from OpenGL import GL


default_model = """
# Blender v2.78 (sub 0) OBJ File: ''
# www.blender.org
o Cube
v 1.000000 -1.000000 -1.000000
v 1.000000 -1.000000 1.000000
v -1.000000 -1.000000 1.000000
v -1.000000 -1.000000 -1.000000
v 1.000000 1.000000 -1.000000
v 1.000000 1.000000 1.000000
v -1.000000 1.000000 1.000000
v -1.000000 1.000000 -1.000000
vt 0.3333 0.6667
vt 0.0000 1.0000
vt 0.0000 0.6667
vt 0.6667 0.6667
vt 0.3333 0.3333
vt 0.6667 0.3333
vt 0.3333 0.3333
vt 0.6667 0.0000
vt 0.6667 0.3333
vt 0.0000 0.0000
vt 0.3333 0.3333
vt 0.0000 0.3333
vt 1.0000 0.3333
vt 0.6667 0.0000
vt 1.0000 0.000
vt 0.3333 0.3333
vt 0.0000 0.6667
vt 0.0000 0.3333
vt 0.3333 1.0000
vt 0.3333 0.6667
vt 0.3333 0.0000
vt 0.3333 0.0000
vt 0.6667 0.3333
vt 0.3333 0.6667
vn 0.0000 -1.0000 0.0000
vn 0.0000 1.0000 0.0000
vn 1.0000 -0.0000 0.0000
vn 0.0000 -0.0000 1.0000
vn -1.0000 -0.0000 -0.0000
vn 0.0000 0.0000 -1.0000
s off
f 2/1/1 4/2/1 1/3/1
f 8/4/2 6/5/2 5/6/2
f 5/7/3 2/8/3 1/9/3
f 6/10/4 3/11/4 2/12/4
f 3/13/5 8/14/5 4/15/5
f 1/16/6 8/17/6 5/18/6
f 2/1/1 3/19/1 4/2/1
f 8/4/2 7/20/2 6/5/2
f 5/7/3 6/21/3 2/8/3
f 6/10/4 7/22/4 3/11/4
f 3/13/5 7/23/5 8/14/5
f 1/16/6 4/24/6 8/17/6
"""


def load_model(fp):
    index = 0

    vertices = []
    uvs = []
    normals = []

    vertex_indices = []
    uv_indices = []
    normal_indices = []

    indexed_vertices = []
    indexed_uvs = []
    indexed_normals = []

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

        indexed_vertices.append(vertex)
        indexed_uvs.append(uv)
        indexed_normals.append(normal)

    return indexed_vertices, indexed_uvs, indexed_normals


fragment_data = """
#version 100
precision highp float;
uniform mat3 ngl_normal_matrix;
uniform sampler2D tex0_sampler;
varying vec3 var_normal;
varying vec2 var_tex0_coords;
void main() {
    vec4 color = texture2D(tex0_sampler, var_tex0_coords);
    gl_FragColor= vec4(vec3(0.5 + color.rgb * var_normal), 1.0);
}
"""


@scene({'name': 'model', 'type': 'model', 'filter': 'Object files (*.obj)'})
def centered_model_media(cfg, n=0.5, model=None):

    try:
        with open(model) as fp:
            vertices, uvs, normals = load_model(fp)
    except:
        import StringIO
        vertices, uvs, normals = load_model(StringIO.StringIO(default_model))

    primitives = []
    for i, vertex in enumerate(vertices):
        uv = uvs[i]
        normal = normals[i]
        primitives.append(ShapePrimitive((
            vertex[0], vertex[1], vertex[2]),
            (uv[0], uv[1]),
            (normal[0], normal[1], normal[2]),
        ))

    q = Shape(primitives)
    m = Media(cfg.media_filename)
    t = Texture(data_src=m)
    s = Shader(fragment_data=fragment_data)
    tshape = TexturedShape(q, s, t)
    tshape.add_glstates(GLState(GL.GL_DEPTH_TEST, GL.GL_TRUE))

    rot = Rotate(tshape, axis=(0,1,0), name="roty")
    rot.add_animkf(AnimKeyFrameScalar(0, 0),
                   AnimKeyFrameScalar(cfg.duration, 360*2))

    camera = Camera(rot)
    camera.set_eye(2.0, 2.0, 2.0)
    camera.set_up(0.0, 1.0, 0.0)
    camera.set_perspective(45.0, cfg.aspect_ratio, 1.0, 10.0)

    return camera
