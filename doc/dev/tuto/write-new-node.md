Writing a new libnopegl node
============================

The vast majority of the code is "node code", in files following the `node_*.c`
pattern. Since nodes need to be able to introspect each others, all their
respective "private" context are declared in a common (not publicly exposed)
header `internal.h`. This header also contains the structures definitions and
prototypes required to implement a new node.

In order to add a node, you need to:

- in `nopegl.h.in`: declare a new `NGL_NODE_*` with an arbitrary unique FourCC
- create a `node_*.c` file declaring a `const struct node_class`. Don't forget
  the Copyright header. See other node files, such as `node_identity.c`
- in `nodes_register.h`: register the new node
- reference the source file node in `libnopegl/meson.build`
- run `make nopegl-updatespecs` (from the top-level Makefile) to update
  `nodes.specs` every time you update the parameters of the node
- similarly, run `make nopegl-updatedoc` to update the [reference
  documentation][libnopegl-ref] after every change to the parameters
- refer to [internal.h][internal-h] for the available callbacks to
  implement in your class map

[libnopegl-ref]: /usr/ref/libnopegl.md
[internal-h]: source:libnopegl/src/internal.h
