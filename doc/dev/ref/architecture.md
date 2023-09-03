Project architecture and organization
=====================================

The `nope.gl` project is split in several parts:

- `libnopegl`: the core of the project, an `OpenGL` engine in `C`, containing
  all the nodes.
- `ngl-tools`: a set of various program tools using `libnopegl`.
- `pynopegl`: a Python binding for `libnopegl` (with the help of `Cython`) to
  create graph scenes in the most simple way.
- `pynopegl-utils`: various Python utilities and examples such as an advanced
  Qt5 controller with many features such as live editing.

## Dependencies

- `libnopegl` requires a standard C toolchain and [meson build system][meson].
  It also depends on [nope.media][nope_media] (libnopemd) for media (video and
  images) playback. [Graphviz][graphviz] is optional but can be used to render
  and preview graphs obtained from the API.
- `ngl-tools` needs [SDL2][sdl2] and `libnopegl` installed.
- `pynopegl` needs [Python][python] and [Cython][cython], and `libnopegl`
  installed.
- `pynopegl-utils` needs [Python][python] and `pynopegl`. The controller depends on
  `PySide6` (which is the main reason why this package is separated from the
  `pynopegl` package). It is also recommended to install [Graphviz][graphviz]
  in order to render graph in the controller.

```{mermaid}
graph
    libnopegl(libnopegl)
    ngl_tools(ngl-tools)
    pynopegl(pynopegl)
    pynopegl_utils(pynopegl-utils)

    cython[Cython]
    ffmpeg[FFmpeg]
    graphviz[Graphviz]
    pyside6[PySide6]
    python[Python]
    sdl2[SDL2]
    libnopemd[libnopemd]
    watchdog[watchdog]

    libnopemd --> ffmpeg
    libnopegl --> libnopemd
    ngl_tools --> sdl2
    ngl_tools --> libnopegl
    ngl_tools -. ngl-python .-> python
    pynopegl --> libnopegl
    pynopegl --> python
    pynopegl --> cython
    pynopegl_utils --> pynopegl
    pynopegl_utils --> python
    pynopegl_utils -.-> graphviz
    pynopegl_utils --> pyside6
    pynopegl_utils --> watchdog

    classDef ngldep fill:#add8e6,color:#222
    classDef extdep fill:#c0c0c0,color:#222
    class libnopegl,ngl_tools,pynopegl,pynopegl_utils ngldep
    class libnopemd,ffmpeg,sdl2,python,cython,graphviz,pyside6,watchdog extdep
```

[meson]: https://mesonbuild.com/
[nope_media]: https://github.com/NopeFoundry/nope.media
[graphviz]: http://www.graphviz.org/
[python]: https://www.python.org/
[cython]: http://cython.org/
[sdl2]: https://www.libsdl.org/
