import inspect
import json
import subprocess
import traceback

def scene(*widgets_specs):
    def real_decorator(scene_func):
        def func_wrapper(*func_args, **func_kwargs):
            return scene_func(*func_args, **func_kwargs)

        final_specs = []

        # Construct a arg -> default dict
        func_specs = inspect.getargspec(scene_func)
        if func_specs.defaults:
            func_defaults = {}
            nb_optionnals = len(func_specs.defaults)
            for i, key in enumerate(func_specs.args[-nb_optionnals:]):
                func_defaults[key] = func_specs.defaults[i]

            # Create a copy of the widgets specifications with the defaults value
            for widget_specs in widgets_specs:
                specs = widget_specs.copy()
                specs['default'] = func_defaults[specs['name']]
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

class NGLMedia:

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

    def _set_media_dimensions(self):
        try:
            data = subprocess.check_output(['ffprobe', '-v', '0',
                                            '-select_streams', 'v:0',
                                            '-of', 'json', '-show_streams',
                                            self._filename])
            data = json.loads(data)
        except:
            print('Error while loading %s: %s' % (self._filename, traceback.format_exc()))
            return (-1, -1)
        st = data['streams'][0]
        self._dimensions = (st['width'], st['height'])

    def __init__(self, filename):
        self._filename = filename
        self._set_media_dimensions()
