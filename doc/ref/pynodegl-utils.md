pynodegl-utils
==============

[pynodegl-utils][pynodegl-utils] provides various utilities around `node.gl`
and its Python binding. Its core tool is the PyQt5 viewer with all its
peripheral features such as the exporter or the tooling for easing the
creation of `node.gl` scene graphs.

[pynodegl-utils]: /pynodegl-utils


## Viewer scenes

Each scene needs to be decorated with the `misc.scene` decorator to be
recognized by the `ngl-viewer`.

**Example**:

```python
from pynodegl_utils.misc import scene

@scene()
def my_scene(cfg):
    ...
```

The first argument of the scene function is always a `cfg` argument is of type
`pynodegl_utils.SceneCfg` and can be read and written by the scene function.
Extra optional arguments to the scene function are allowed.

Every scene must return a valid `pynodegl` node object.


## Viewer widgets

Widgets are specified as named `dict` arguments to the `@misc.scene` decorator.
The `@misc.scene()` arguments must match the name of the corresponding argument
in the scene construction function.

**Note**: it is not mandatory to create a widget definition for every optional
scene function argument.

**Example**:

```python
@scene(foo={'type': ...}, bar={'type': ...})
def example(cfg, foo=..., bar=...):
    ...
```

List of available widget types:

### range

A `range` is associated with a **slider** widget.

Available options:

Option      | Description
----------- | -----------
`range`     | the range itself, as a `list` or a `tuple` of 2 values
`unit_base` | `1/unit_base` corresponds to the sliders step

The associated argument value is expected to be a scalar value.

**Example**:

```python
@scene(n={'type': 'range', 'range': [0, 5], 'unit_base': 100})
def range_example(cfg, n=2.5):
    ...
```

![range widget](img/widget-range.png)

### color

A `color` is associated with a **color picker** widget.

The associated argument value is expected to be a 4-value `tuple` or `list`.

**Example**:

```python
@scene(bgcolor={'type': 'color'})
def color_example(cfg, bgcolor=(0.3, 0.4, 0.5, 1.0)):
    ...
```

![color widget](img/widget-color.png)

### bool

A `bool` is associated with a **check box** widget.

The associated argument is expected to be a `bool`.

**Example**:

```python
@scene(bilinear={'type': 'bool'})
def bool_example(cfg, bilinear=True):
    ...
```

![bool widget](img/widget-bool.png)

### file

A `file` is associated with a **file chooser** widget.

The associated argument is expected to be a `str` or `None`, corresponding to
the file path.

Available options:

Option      | Description
----------- | -----------
`filter`    | a `str` identifying the type of file supported (refer to the `QtWidgets.QFileDialog` documentation for more details)

**Example**:

```python
@scene(input={'type': 'file', 'filter': 'Text files (*.txt)'})
def file_example(cfg, input=None):
    ...
```

![file widget](img/widget-file.png)


## Viewer hooks

When using the `--hooks-dir` option, `ngl-viewer` will execute various hook
according to various events. These hooks are typically used for triggering a
synchronization with another device.

Following are the hook script or programs that will be executed and their
expected behaviour.

### hook.get_remote_dir

`hook.get_remote_dir` does not take any argument. It must print on `stdout` the
remote path where the scene files are supposed to be synchronized.

This hook is not mandatory.

See also `hook.sync`.

**Example**:

```shell
$ android/hook.get_remote_dir
/sdcard/nodegl_data
```

### hook.get_gl_backend

`hook.get_gl_backend` does not take any argument. It must print on `stdout` the
string `gl` or `gles`. It corresponds to the OpenGL backend specified in the
scene configuration when constructed.

This hook is not mandatory.

**Example**:

```shell
$ android/hook.get_gl_backend
gles
```

### hook.get_system

`hook.get_system` does not take any argument. It must print on `stdout` a
string identifying the operating system.

Accepted values are:

- `Linux`
- `Android`
- `Darwin`
- `iOS`

This hook is not mandatory.

**Example**:

```shell
$ android/hook.get_system
Android
```

### hook.sync

`hook.sync` takes 2 arguments:

1. `localfile`: the path to the local file
2. `remotefile`: the path to the remote file

It is called for every file to sync (typically media files).

This hook is not mandatory.

See also `hook.get_remote_dir`.

**Example**:

```shell
$ android/hook.sync /tmp/ngl-media.mp4 /sdcard/nodegl_data
/tmp/ngl-media.mp4: 1 file pushed. 14.3 MB/s (1564947 bytes in 0.105s)
```

### hook.scene_change

`hook.scene_change` takes several arguments:

- first argument is the `localscene`: the path to the local serialized scene
- every following argument is a key-value string following the format
  `key=value`. Available named variables are following:
  - `duration`: expressed as a float (in seconds)
  - `aspect_ratio`: expressed as a fraction in format `num/den` where `num` and `den` are `int`
  - `framerate`: expressed as a fraction in format `num/den` where `num` and `den` are `int`
  - `clear_color`: expressed as a 32-bit hexadecimal following the `RRGGBBAA` format

This hook is mandatory if anything is expected to happen when using hooks.

**Example**:

A call from `ngl-viewer` to this hook will look like this:

```shell
hook.scene_change /tmp/ngl_scene.ngl duration=5 framerate=60000/1001 aspect_ratio=16/9 clear_color=4A646BFF
```
