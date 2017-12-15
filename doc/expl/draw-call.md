What happens in a draw call?
============================

The `ngl_draw()` call is split in 4 passes:

1. Visit the graph
2. Honor node resources (prefetch and release)
3. Update the nodes
4. Actual GL drawing


## Visit

By default, `ngli_node_visit()` will crawl every node parameter to find the
child nodes. It is meant to flag every node as *active* or *inactive* for a
given time. The final state of a node depends on the knowledge of the whole
graph because of diamond shaped tree. A typical case is an active branch and an
inactive branch sharing a common node; that leaf node must stay active even
though one its parent is inactive.

**Note**: if a branch is already inactive, the visitor is unlikely to follow it
down, preventing an huge overhead on the tree with a large number of time range
filtered branches.


## Prefetch/Release

The second pass, performed by `ngli_node_honor_release_prefetch()` will crawl
every node previously visited by the first pass and execute a prefetch or a
release according to how it's been flagged. Previously unvisited nodes (and
their children) are ignored.

For medias, prefetching typically means starting to open the file in advance so
it's ready to playback immediately when its time arrive. Similarly, textures
are allocated or released accordingly.


## Update

The 3rd pass is executing a cascading update with `ngli_node_update()`. Each
node decides how it updates its state and trigger the update of its children
for a given time.


## Draw

The 4th and final pass executed by `ngli_node_draw()` is similar to update in
the way it is cascading (explicit in every node). This pass does not require
time, its only purpose lies in drawing to the screen.
