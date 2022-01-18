GRAPH_TOOL = dot

all: binding.png

%.png: %.dot
	$(GRAPH_TOOL) $< -Tpng > $@
