GRAPH_TOOL = dot

all: dependencies.png

%.png: %.dot
	$(GRAPH_TOOL) $< -Tpng > $@
