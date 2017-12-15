Writing a new libnodegl node
============================

The vast majority of the code is "node code", in files following the `node_*.c`
pattern. Since nodes need to be able to introspect each others, all their
respective "private" context are declared in a common (not publicly exposed)
header `nodes.h`. This header also contains the structures definitions and
prototypes required to implement a new node.

In order to add a node, you need to:

- in `nodegl.h`: declare a new `NGL_NODE_*` with an arbitrary unique FourCC
- create a `node_*.c` file declaring a `const struct node_class`. Don't forget
  the Copyright header. See other node files, such as `node_identity.c`
- in `nodes_register.h`: register the new node
- in `Makefile`: add the object name associated with your `.c` to `LIB_OBJS`
- run `make updatespecs` to update `nodes.specs` every time you update the
  parameter of the node
- similarly, run `make updatedoc` to update the [reference
  documentation][libnodegl-ref] after every change to the parameters
- refer to [nodes.h][nodes-h] for the available callbacks to
  implement in your class map

[libnodegl-ref]: /libnodegl/doc/libnodegl.md
[nodes-h]: /libnodegl/nodes.h
