Project architecture and organization
=====================================

The `node.gl` project is split in several parts:

- `libnodegl`: the core of the project, an `OpenGL` engine in `C`, containing
  all the nodes.
- `ngl-tools`: a set of various program tools using `libnodegl`.
- `pynodegl`: a Python binding for `libnodegl` (with the help of `Cython`) to
  create graph scenes in the most simple way.
- `pynodegl-utils`: various Python utilities and examples such as an advanced
  Qt5 controller with many features such as live editing.

## Dependencies

- `libnodegl` requires a standard C toolchain and [meson build system][meson].
  It also depends on [sxplayer library][sxplayer] for media (video and images)
  playback. [Graphviz][graphviz] is optional but can be used to render and
  preview graphs obtained from the API.
- `ngl-tools` needs [SDL2][sdl2] and `libnodegl` installed.
- `pynodegl` needs [Python][python] and [Cython][cython], and `libnodegl`
  installed.
- `pynodegl-utils` needs [Python][python] and `pynodegl`. The controller depends on
  `PySide6` (which is the main reason why this package is separated from the
  `pynodegl` package). It is also recommended to install [Graphviz][graphviz]
  in order to render graph in the controller.

```mermaid
graph
    libnodegl(libnodegl)
    ngl_tools(ngl-tools)
    pynodegl(pynodegl)
    pynodegl_utils(pynodegl-utils)

    cython[Cython]
    ffmpeg[FFmpeg]
    graphviz[Graphviz]
    pyside6[PySide6]
    python[Python]
    sdl2[SDL2]
    sxplayer[sxplayer]
    watchdog[watchdog]

    sxplayer --> ffmpeg
    libnodegl --> sxplayer
    ngl_tools --> sdl2
    ngl_tools --> libnodegl
    ngl_tools -. ngl-python .-> python
    pynodegl --> libnodegl
    pynodegl --> python
    pynodegl --> cython
    pynodegl_utils --> pynodegl
    pynodegl_utils --> python
    pynodegl_utils -.-> graphviz
    pynodegl_utils --> pyside6
    pynodegl_utils --> watchdog

    classDef ngldep fill:#add8e6,color:#222
    classDef extdep fill:#c0c0c0,color:#222
    class libnodegl,ngl_tools,pynodegl,pynodegl_utils ngldep
    class sxplayer,ffmpeg,sdl2,python,cython,graphviz,pyside6,watchdog extdep
```

[meson]: https://mesonbuild.com/
[sxplayer]: https://github.com/gopro/sxplayer
[graphviz]: http://www.graphviz.org/
[python]: https://www.python.org/
[cython]: http://cython.org/
[sdl2]: https://www.libsdl.org/
