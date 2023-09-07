# Displaying content using Render nodes

`Render*` nodes can be used to display all kind of content: solid colors,
gradients, multimedia files, ...

```{nope} renders.gradient
:export_type: image
A simple horizontal gradient from teal to orange
```

```{nope} renders.image
:export_type: image
Render an image
```

```{nope} renders.video
:export_type: video
Render a video
```

```{nope} renders.noise
:export_type: video
Render an animated fractal noise
```

## Render with custom shaders

It is also possible to create a custom shader using the [Render] node and get a
similar experience to [shadertoy](https://www.shadertoy.com).

In the following scene, even if the shaders are loaded from an external file
(for example with `frag = open("/path/to/shader.frag").read()`, the viewer
will automatically detect the `open()` and reload the preview when the file
is changed.

To do texture picking, add the `Texture2D` to the `frag_resources`, and use
`ngl_texvideo()` in the shader. See [this shaders explanation section][shadertex]
for more information.

```{nope} renders.shadertoy
:export_type: video
Custom shader development environment
```

[Render]: /usr/ref/libnopegl.md#render
[shadertex]: /usr/expl/shaders.md#textures
