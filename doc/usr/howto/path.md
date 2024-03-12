# Animating and displaying paths

The [Path] node can be used to build animations along complex shapes and can
be displayed with the help of [DrawPath]. It take various `PathKey*` nodes as
input to describe the path itself.

Rendering a path with a single cubic curve looks like this:

```{nope} path.simple
:export_type: image
A single cubic curve
```

The usage is fairly similar to the interface used in the SVG specifications for
building paths. Typically, curves can be chained together to make more complex
shapes:

```{nope} path.chain
:export_type: image
A path composed of a serie of quadratic and cubic curves
```

A path can also be composed of several subpaths (by inserting move keys):

```{nope} path.subpaths
:export_type: image
A path composed of several sub-paths
```

Here the 2nd path is going counter-clockwise (compared to the clockwise
outline), so it causes a subtraction.

While a path can be rendered, it can also be used to animate elements:

```{nope} path.animated
:export_type: video
Animation of an element along a path
```

Finally, [DrawPath] has a bunch of effects such as glowing, and like any other
drawing nodes it can be transformed at will:

```{nope} path.effects
:export_type: video
Glowing beating heart
```

The [Path] node isn't the only node that can generate a path, [SmoothPath] is
another one where instead of specifying the curves, only points to go through
need to be specified (with two extra controls at the extremities).

[Path]: /usr/ref/libnopegl.md#path
[SmoothPath]: /usr/ref/libnopegl.md#smoothpath
[DrawPath]: /usr/ref/libnopegl.md#drawpath
