node.gl
=======

`node.gl` is a [GoPro][gopro] OpenGL engine for building and rendering
graph-based scenes. It is designed to run on both desktop (Linux, OSX, Windows)
and mobile (Android, iOS).

The `node.gl` project is split in 3 parts:

- `libnodegl`: the core of the project, an `OpenGL` engine in `C`
- `pynodegl`: a Python binding for `libnodegl` (with the help of `Cython`)
- `pynodegl-utils`: various Python utilities and examples such as a Qt5 viewer

*Warning:* note that `node.gl` is still highly experimental. This means the ABI
and API can change at any time.

[gopro]: https://gopro.com/

## Dependencies

- `libnodegl` requires a standard C toolchain (C compiler, linker, GNU/Make).
  It also depends on [sxplayer library][sxplayer] for media (video and images)
  playback. [Graphviz][graphviz] is optional but can be used to render and
  preview graphs obtained from the API.
- `pynodegl` needs [Python][python] and [Cython][cython], and `libnodegl`
  installed.
- `pynodegl-utils` needs [Python][python] and `pynodegl`. The viewer depends on
  `PyQt5` (which is the main reason why this package is separated from the
  `pynodegl` package). It is also recommended to install [Graphviz][graphviz]
  in order to render graph in the viewer.

[sxplayer]: https://github.com/stupeflix/sxplayer
[graphviz]: http://www.graphviz.org/
[python]: https://www.python.org/
[cython]: http://cython.org/

## Installation of `libnodegl` (the core library)

### Build

`make` is enough to build `libnodegl.a`.

If you prefer a dynamic library, you can use the variable `SHARED`, such as
`make SHARED=yes`.

If you need symbol debugging, you can use `make DEBUG=yes`.

Make allow options to be combinable, so `make SHARED=yes DEBUG=yes` is valid.

Additionally, `PYTHON` and `PKG_CONFIG` which respectively allows to customize
`python` and `pkg-config` executable paths.

### Installation

`make install` will install the library in `PREFIX`, which you can override,
for example using `make install PREFIX=/tmp/local`.

You can check the installed version of `libnodegl` using `pkg-config
--modversion libnodegl`

## Installation of `pynodegl` (the Python binding)

`pip install ./pynodegl`

## Installation of `pynodegl-utils` (the Python utilities and examples)

```
pip install -r ./pynodegl-utils/requirements.txt
pip install ./pynodegl-utils
```

## License

`node.gl` is licensed under the Apache License, Version 2.0. Read the `LICENSE`
and `NOTICE` files for details.

**Warning**: `pynodegl-utils` has an optional dependency on PyQt5 which is
licensed under the GPL and thus restrict the `pynodegl-utils` module
distribution.

## Using the API

All the API is defined in the installed header `nodegl.h`. You can consult the
nodes parameters in the `nodes.specs` file installed in the data root dir
(usually something along the lines `/usr/share/nodegl/nodes.specs`).

## Contributing

See `DEVELOPERS.md` file.
