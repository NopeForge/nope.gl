Developer guidelines
====================

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

- **DO NOT** reorder public enum such as `NGL_PLATFORM_*` as it breaks ABI
- **DO NOT** insert a value in the middle of a public enum as it breaks ABI
- **DO NOT** remove or change a public prototype as it breaks API and ABI
- **DO NOT** add ANY external dependency without previous discussion
- **DO NOT** use tabs (except in Makefile) and make sure not to add trailing
  whitespaces
- **DO NOT** create ANY merge commit in the history, learn how to use `git
  rebase`
- **DO NOT** add big (as in > 200kB) binary files to the repository
