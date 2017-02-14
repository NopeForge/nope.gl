Developers guidelines
=====================

In order to keep the API maintainable in the long term, we try to keep it as
small as possible. This means the only public header of the project `nodegl.h`
must be kept with as little symbols as possible.

DO
--

- **DO** Keep It Simple, Stupid
- **DO** follow the coding style you observe in the existing files
- **DO** atomic commits with the appropriate prefix (look at the git history)

DO NOT
------

- **do NOT** reorder public enum such as `NGL_NODE_*` as it breaks ABI
- **do NOT** insert a value in the middle of a public enum as it breaks ABI
- **do NOT** remove or change a public prototype as it breaks API and ABI
- **do NOT** add ANY external dependency without previous discussion
- **do NOT** use tabs (except in Makefile) and make sure not to add trailing
  whitespaces
- **do NOT** create ANY merge commit in the history, learn how to use `git
  rebase`
- **do NOT** add big (as in > 200kB) binary files to the repository

Nodes
-----

The vast majority of the code is "node code", in files following the `node_*.c`
pattern. Since nodes need to be able to introspect each others, all their
respective "private" context are declared in a common (not publicly exposed)
header `nodes.h`. This header also contains the structures definitions and
prototypes required to implement a new node.

In order to add a node, you need to:

- in `nodegl.h`: declare a new `NGL_NODE_*` at **THE END** of the appropriate enum
- create a `node_*.c` file declaring a `const struct node_class`. Don't forget
  the Copyright header. See other node files, such as `node_identity.c`
- in `nodes.c`: declare the class you just create as `extern`
- in `nodes.c`: add a pointer to that class in `node_class_map`
- in `Makefile`: add the object name associated with your `.c` to `LIB_OBJS`
- run `make updatespecs` to update `nodes.specs` every time you update the
  parameter of the node
- refer to `nodes.h` for the available callbacks to implement in your class map
