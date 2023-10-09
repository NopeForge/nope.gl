# 🔰 Starter tutorial

## Installing the viewer

The viewer is the most accessible tool for working with the framework. The
simplest way to  grab it is to download the last [pre-build of it on the
Releases page][releases].

An alternative to this is to build the development environment without
installing anything on the system, more specifically: the [first method of the
installation guide][install]. In a nutshell: `./configure.py && make` creates a
complete Python-based working environment. Within that virtual environment, the
`ngl-viewer` command is available.

[releases]: https://github.com/NopeForge/nope.gl/releases/
[install]: /usr/howto/installation.md

## Creating a simple scene in Python

Edit a new script such as `mydemo.py` and add the following code:

```{nope} starter.hello_world
:export_type: image
Hello World!
```

Loading that script in the viewer should provide the same rendering observed
here.

In this first example, all we did was to declare a function that returns a
single node (the [Text] node). The `@ngl.scene()` decorator is used to make
it discoverable by the viewer. The function receives a configuration of type
`SceneCfg` which gives us access to various metadata of the scene such as
`aspect_ratio`, `duration` or `framerate` (for both reading and sometimes
writing).

Try to modify the text in the script: the viewer should automatically update
the preview.

## Making a simple composition

To add a background to our scene, we need to create a composition where we first
draw that background, then our text. For the background, we can pick one of the
proposition from the [renders howtos]. For the composition itself, we can rely
on the [Group] node to split the graph in two branches:

```{nope} starter.bg_fg_composition
:export_type: image
Simple background and text composition
```

Looking at the graph tab should clarify the structure constructed here.

You may have noticed that we did change the `Text.bg_opacity` parameter. Try to
play with that value (somewhere between 0 and 1) to see how it impacts the scene.

## Animating values

In many cases, node parameters that accept constant values also accept nodes.
We can see that it's the case with [RenderGradient] parameters (look for the
`node` flag), and notably `color1` and `color2` we're currently using in our
scene. Accepting nodes means we could animate these parameters:

```{nope} starter.animated
:export_type: video
Animating the background colors
```

`color0_animkf` and `color1_animkf` define time key frames. Each of these time
key frames associate a time with a value, for which the engine interpolates the
intermediate values for a given time. The interpolations are linear by default,
but you can customize that. More specifically, try to change the `easing`
parameter of the [AnimkeyFrameColor] nodes (but the first one) to adjust the
rhythm of the scene.

You may also have noticed in the [RenderGradient] documentation that the
positions of the gradients could be changed. You could try to do that with the
help of an [AnimatedVec2] and [AnimKeyFrameVec2].

## Transforming geometries

The object geometries can be modified using various transformation nodes. You
can find a few examples in the [transforms howtos]. They can be nested just like
functions, so if we want to scale down *then* rotate, we would express it with
`Rotate(Scale(target))`. Let's do that to our text node:

```{nope} starter.transforms
:export_type: video
Apply 2 transforms to the text
```

Again, looking at the graph tab may clarify the structure constructed here.

## Exposing parameters

Some node parameters have the `live` flag. This means we can expose a
live-control up to the GUI. Try for example to add `live_id="my_text"` to the
`Text` node and see how it appears in the viewer.

## Controlling the visibility of objects

For this last subsection, we are going to see how to control when things are
visible or not. We will be using a [TimeRangeFilter] to make a shape shortly
appears behind the text.

```{nope} starter.timeranges
:export_type: video
Time controlled shape
```

## Diving into the documentation

From here, if you you're looking at a specific area, you may want to look at
the [how-to guides][howto].

If you need to understand the *why* of certain design decisions or limitations,
the [discussions and explanations][expl] section will come to an help.

Finally, in every situation, you will feel the need to check out the [reference
documentation][refdoc] for austere but exhaustive information, and in
particular, [all the node definitions][ref-libnopegl].

[renders howtos]: /usr/howto/renders.md
[transforms howtos]: /usr/howto/transforms.md
[Text]: /usr/ref/libnopegl.md#text
[Group]: /usr/ref/libnopegl.md#group
[RenderGradient]: /usr/ref/libnopegl.md#rendergradient
[AnimKeyFrameColor]: /usr/ref/libnopegl.md#animkeyframecolor
[AnimatedVec2]: /usr/ref/libnopegl.md#animatedvec2
[AnimKeyFrameVec2]: /usr/ref/libnopegl.md#animkeyframevec2
[TimeRangeFilter]: /usr/ref/libnopegl.md#timerangefilter
[howto]: /usr/howto/index.md
[expl]: /usr/expl/index.md
[refdoc]: /usr/ref/index.md
[ref-libnopegl]: /usr/ref/libnopegl.md
