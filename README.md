node.gl
=======

`node.gl` is a [GoPro][gopro]/[Stupeflix][stupeflix] OpenGL engine for building
and rendering graph-based scenes. It is designed to run on both desktop (Linux,
OSX) and mobile (Android, iOS).

*Warning:* note that `node.gl` is still highly experimental. This means the ABI
and API can change at any time.

[gopro]: https://gopro.com/
[stupeflix]: https://stupeflix.com/

## Dependencies

`node.gl` is written in C11 and needs [Python][python] and [Cython][cython] to
build its Python binding.

It also depends on [sxplayer library][sxplayer] for media (video and images)
playback.

It is recommended to install [Graphviz][graphviz] in order to be able to render
and preview graph (using the `viewer.py` demo script or using the API).

[python]: https://www.python.org/
[cython]: http://cython.org/
[sxplayer]: https://github.com/stupeflix/sxplayer
[graphviz]: http://www.graphviz.org/

## License

`node.gl` is licensed under the Apache License, Version 2.0. Read the `LICENSE`
file for details.

## Demo

You can get an overview of the engine by running
`make SHARED=yes pynodegl.so && python2 ./viewer.py /path/to/a/video.mkv`

The examples imported by the viewer are located in the `examples` directory.

## Using the API

All the API is defined in the installed header `nodegl.h`. You can consult the
nodes parameters in the `nodes.specs` file.
