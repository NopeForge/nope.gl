# Displaying content using drawing nodes

`Draw*` nodes can be used to display all kind of content: solid colors,
gradients, multimedia files, ...

```{nope} draws.gradient
:export_type: image
A simple horizontal gradient from teal to orange
```

```{nope} draws.image
:export_type: image
Draw an image
```

```{nope} draws.video
:export_type: video
Draw a video
```

```{nope} draws.noise
:export_type: video
Draw an animated fractal noise
```

## Draw with custom shaders

It is also possible to create a custom shader using the [Draw] node and get a
similar experience to [shadertoy](https://www.shadertoy.com).

In the following scene, even if the shaders are loaded from an external file
(for example with `frag = open("/path/to/shader.frag").read()`, the viewer
will automatically detect the `open()` and reload the preview when the file
is changed.

To do texture picking, add the `Texture2D` to the `frag_resources`, and use
`ngl_texvideo()` in the shader. See [this shaders explanation section][shadertex]
for more information.

```{nope} draws.shadertoy
:export_type: video
Custom shader development environment
```

[Draw]: /usr/ref/libnopegl.md#draw
[shadertex]: /usr/expl/shaders.md#textures
