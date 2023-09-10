# Creating transforms

There are 4 basic transform nodes: [Translate], [Scale], [Rotate] and [Skew]. It
is also possible to create custom transform with the [Transform] node (by
passing down a 4x4 matrix). They can be used to transform the vertices of the
shapes below in the graph.

```{nope} transforms.rotate
:export_type: image
Rotate a quad by 45°
```

```{nope} transforms.animated_rotate
:export_type: video
Rotate a quad by 360° over the course of 3 seconds
```

```{nope} transforms.animated_translate
:export_type: video
Translate a circle from one side to another, and back
```

```{nope} transforms.animated_scale
:export_type: video
Scale up and down a circle
```

[Translate]: /usr/ref/libnopegl.md#translate
[Scale]: /usr/ref/libnopegl.md#scale
[Rotate]: /usr/ref/libnopegl.md#rotate
[Skew]: /usr/ref/libnopegl.md#skew
[Transform]: /usr/ref/libnopegl.md#transform
