import os.path as op
import platform
import math
import inspect
import json
import subprocess


def scene(**widgets_specs):
    def real_decorator(scene_func):
        def func_wrapper(idict=None, **extra_args):

            if idict is None:
                idict = {}

            fields = (
                'aspect_ratio',
                'duration',
                'framerate',
                'glbackend',
                'samples',
                'system',
            )

            scene_cfg = SceneCfg(idict.get('medias'))
            for field in fields:
                if field in idict:
                    setattr(scene_cfg, field, idict[field])

            scene = scene_func(scene_cfg, **extra_args)

            odict = {}
            for field in fields:
                odict[field] = getattr(scene_cfg, field)
            odict['medias'] = scene_cfg.medias
            odict['scene'] = scene

            return odict

        final_specs = []

        # Construct a arg -> default dict
        func_specs = inspect.getargspec(scene_func)
        if func_specs.defaults:
            nb_optionnals = len(func_specs.defaults)
            for i, key in enumerate(func_specs.args[-nb_optionnals:]):
                # Create a copy of the widgets specifications with the defaults value
                if key in widgets_specs:
                    specs = widgets_specs[key].copy()
                    specs['name'] = key
                    specs['default'] = func_specs.defaults[i]
                    final_specs.append(specs)

        # Transfers the widgets specifications to the UI.
        # We could use the return value but it's better if the user can still
        # call its decorated scene function transparently inside his own code
        # without getting garbage along the return value.
        func_wrapper.widgets_specs = final_specs

        # Flag the scene as a scene function so it's registered in the UI.
        func_wrapper.iam_a_ngl_scene_func = True

        return func_wrapper

    return real_decorator


def get_shader(filename, shader_path=None):
    if shader_path is None:
        shader_path = op.join(op.dirname(__file__), 'examples', 'shaders')
    return open(op.join(shader_path, filename)).read()


def get_frag(name, shader_path=None):
    return get_shader(name + '.frag', shader_path)


def get_vert(name, shader_path=None):
    return get_shader(name + '.vert', shader_path)


def get_comp(name, shader_path=None):
    return get_shader(name + '.comp', shader_path)


class Media:

    @property
    def filename(self):
        return self._filename

    @property
    def width(self):
        return self._dimensions[0]

    @property
    def height(self):
        return self._dimensions[1]

    @property
    def dimensions(self):
        return self._dimensions

    @property
    def duration(self):
        return self._duration

    def _set_media_dimensions(self):
        data = subprocess.check_output(['ffprobe', '-v', '0',
                                        '-select_streams', 'v:0',
                                        '-of', 'json',
                                        '-show_streams', '-show_format',
                                        self._filename])
        data = json.loads(data)
        st = data['streams'][0]
        self._dimensions = (st['width'], st['height'])
        self._duration = float(data['format']['duration'])

    def __init__(self, filename):
        self._filename = filename
        self._set_media_dimensions()


class SceneCfg:

    LOOP_DURATION = 30.0
    FRAME_RATE = (60, 1)
    DEFAULT_MEDIA_FILE = '/tmp/ngl-media.mp4'

    def __init__(self, medias=None):
        if medias is None:
            media_file = self.DEFAULT_MEDIA_FILE
            if not op.exists(self.DEFAULT_MEDIA_FILE):
                ret = subprocess.call(['ffmpeg', '-nostdin', '-nostats', '-f', 'lavfi', '-i',
                                       'testsrc2=d=%d:r=%d/%d' % (int(math.ceil(self.LOOP_DURATION)),
                                                                  self.FRAME_RATE[0], self.FRAME_RATE[1]),
                                       media_file])
                if ret:
                    raise Exception("Unable to create a media file using ffmpeg (ret=%d)" % ret)
            medias = [Media(media_file)]

        self.medias = medias
        self.duration = self.LOOP_DURATION
        self.aspect_ratio = (16, 9)
        self.framerate = self.FRAME_RATE
        self.samples = 0
        self.glbackend = 'gl'
        self.system = platform.system()
