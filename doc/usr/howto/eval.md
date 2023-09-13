# Using Eval* nodes

`Eval*` nodes are useful to evaluate various expressions during the graph
evaluation. The complete list of operators, constants and functions can be
consulted in the [Eval reference documentation][eval-ref].

Each node can be nested to create complex interwined relationships, and they are
not limited to float and vectors: `Noise*` or `Color*` nodes are also accepted
as input resources.

For example, the following example demonstrates how to animate the position of
the 2 gradient positions:

```{nope} eval.gradient
:export_type: video
Animate a gradient and objects using CPU evaluation
```

This previous example shows how we can craft positions, but also colors. The
eval API proposes a few helper to work with colors, so we could use it for
example to build an entire palette and its grayscale variant at runtime (so that
it can reacts to live-controls):

```{nope} eval.palette_strip
:export_type: image
A palette strip and its grayscale version, interpolated from 2 colors
```

Here we're using `luma()` to get the greyscale values, and `srgbmix()` to
interpolate between 2 sRGB colors (interpolation is done in linear space).

Of course, if the interpolation is not required at runtime (ie: the result does
not depend on the time or live controls values), it is highly recommended to
precompute them from the Python instead to have it computed only once.

[eval-ref]: /usr/ref/eval.md
