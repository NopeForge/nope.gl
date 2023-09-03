pynopegl
========

[pynopegl][pynopegl] is the Python binding for [libnopegl][libnopegl]. It is
automatically generated from to the `libnopegl` node specifications file.  For
detailed information on how the `pynopegl` binding is created, refer to the
corresponding [pynopegl explanation][expl-pynopegl].

In order to use the `pynopegl`, you will have to rely on the [reference node
specifications][ref-libnopegl]. The Python binding classes and parameters are
directly named after the names found in the specifications. For example, to get
an instance of a [Group][ref-libnopegl-group], one has to instantiate a `Group`
such as `g = Group(children=(r1, r2, r3))`.

## Parameters in constructors

In the constructors, the expected Python parameter types follow this table:

Parameter type          | Python type                         | Example
----------------------- | ----------------------------------- | ------------
`*Dict`                 | `dict`                              | `render = Render(geometry, frag_resources={'tex0': t0, 'tex1': t1})`
`vec*`, `mat*`, `*List` | Iterable such as `tuple` or `list`  | `scale = Scale(child, factors=(0.3, 1.2, 1.0))`
All the others          | `-`                                 | `circle = Circle(radius=0.5, npoints=128)`

## Parameter type to method correspondence

Every parameter also has a setter method associated, named according to the
parameter name and its type.

Parameter type | Node method                      | Python <param> type        | Example
-------------- | -------------------------------- | -------------------------- | --------
`*Dict`        | `update_<param>(self, <param>)`  | `dict` or named arguments  | `render.update_frag_resources(tex0=t0, tex1=t1)`
`*List`        | `add_<param>(self, *<param>)`    | positional arguments       | `group.add_children(r1, r2, r3)`
All the others | `set_<param>(self, <param>)`     | positional arguments       | `camera.set_center(1.0, -1.0, 0.5)`

[pynopegl]: source:pynopegl
[libnopegl]: source:libnopegl
[expl-pynopegl]: /dev/expl/pynopegl.md
[ref-libnopegl]: /usr/ref/libnopegl.md
[ref-libnopegl-group]: /usr/ref/libnopegl.md#group
